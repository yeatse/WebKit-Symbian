#include "PageList.h"

#include "BrowserLog.h"

#include <stdio.h>
#include <string.h>

HBufC8* ToUtf8LC(const TDesC& aText)
{
    HBufC8* utf8 = HBufC8::NewLC(aText.Length() * 3 + 1);
    TPtr8 utf8Ptr = utf8->Des();
    for (TInt i = 0; i < aText.Length(); ++i) {
        TUint c = aText[i];
        if (c < 0x80)
            utf8Ptr.Append(c);
        else if (c < 0x800) {
            utf8Ptr.Append(0xC0 | (c >> 6));
            utf8Ptr.Append(0x80 | (c & 0x3F));
        } else {
            utf8Ptr.Append(0xE0 | (c >> 12));
            utf8Ptr.Append(0x80 | ((c >> 6) & 0x3F));
            utf8Ptr.Append(0x80 | (c & 0x3F));
        }
    }
    utf8Ptr.ZeroTerminate();
    return utf8;
}

namespace {

// UTF-8 to UTF-16, the reverse of ToUtf8LC(). Tabs and line breaks, which
// separate the fields and the lines of the file, become spaces.
HBufC* FromUtf8LC(const TDesC8& aText)
{
    HBufC* text = HBufC::NewLC(aText.Length());
    TPtr textPtr = text->Des();
    for (TInt i = 0; i < aText.Length();) {
        TUint c = aText[i++];
        TInt following = 0;
        if (c >= 0xF0) {
            c &= 0x07;
            following = 3;
        } else if (c >= 0xE0) {
            c &= 0x0F;
            following = 2;
        } else if (c >= 0xC0) {
            c &= 0x1F;
            following = 1;
        } else if (c >= 0x80)
            c = 0xFFFD;
        for (; following && i < aText.Length() && (aText[i] & 0xC0) == 0x80; --following)
            c = (c << 6) | (aText[i++] & 0x3F);
        if (following)
            c = 0xFFFD;
        if (c >= 0x10000) {
            // Two UTF-16 units from four UTF-8 bytes: the text cannot grow.
            c -= 0x10000;
            textPtr.Append(0xD800 | (c >> 10));
            textPtr.Append(0xDC00 | (c & 0x3FF));
        } else
            textPtr.Append(c == '\t' || c == '\r' || c == '\n' ? ' ' : c);
    }
    return text;
}

HBufC* OneLineLC(const TDesC& aText)
{
    HBufC* text = aText.AllocLC();
    TPtr textPtr = text->Des();
    for (TInt i = 0; i < textPtr.Length(); ++i) {
        if (textPtr[i] == '\t' || textPtr[i] == '\r' || textPtr[i] == '\n')
            textPtr[i] = ' ';
    }
    textPtr.Trim();
    return text;
}

} // namespace

CPageList* CPageList::NewL(const char* aPath, TInt aMaxCount)
{
    CPageList* self = new (ELeave) CPageList(aMaxCount);
    CleanupStack::PushL(self);
    self->ConstructL(aPath);
    CleanupStack::Pop(self);
    return self;
}

CPageList::CPageList(TInt aMaxCount)
    : iMaxCount(aMaxCount)
{
}

CPageList::~CPageList()
{
    for (TInt i = 0; i < iPages.Count(); ++i) {
        delete iPages[i].iUrl;
        delete iPages[i].iTitle;
    }
    iPages.Close();
    delete iPath;
}

void CPageList::ConstructL(const char* aPath)
{
    TPtrC8 path(reinterpret_cast<const TUint8*>(aPath));
    iPath = HBufC8::NewL(path.Length() + 1);
    iPath->Des().Copy(path);
    iPath->Des().ZeroTerminate();
    // A file that cannot be read gives an empty list, not a browser that
    // does not start.
    TRAPD(error, ReadL());
    if (error)
        BrowserLog("%s: cannot read (%d)", aPath, error);
}

void CPageList::ReadL()
{
    FILE* file = fopen(reinterpret_cast<const char*>(iPath->Ptr()), "rb");
    if (!file)
        return;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    HBufC8* contents = size > 0 ? HBufC8::New(size) : nullptr;
    if (!contents) {
        fclose(file);
        return;
    }
    CleanupStack::PushL(contents);
    TPtr8 contentsPtr = contents->Des();
    contentsPtr.SetLength(fread(const_cast<TUint8*>(contentsPtr.Ptr()), 1, size, file));
    fclose(file);

    TPtrC8 rest(contentsPtr);
    while (rest.Length() && (iMaxCount <= 0 || iPages.Count() < iMaxCount)) {
        TInt end = rest.Locate('\n');
        TPtrC8 line = end == KErrNotFound ? rest : rest.Left(end);
        rest.Set(end == KErrNotFound ? TPtrC8() : rest.Mid(end + 1));
        if (line.Length() && line[line.Length() - 1] == '\r')
            line.Set(line.Left(line.Length() - 1));
        TInt tab = line.Locate('\t');
        TPtrC8 url = tab == KErrNotFound ? line : line.Left(tab);
        if (!url.Length())
            continue;
        TPage page;
        page.iUrl = FromUtf8LC(url);
        page.iTitle = FromUtf8LC(tab == KErrNotFound ? TPtrC8() : line.Mid(tab + 1));
        iPages.AppendL(page);
        CleanupStack::Pop(2, page.iUrl);
    }
    CleanupStack::PopAndDestroy(contents);
}

// Writes the whole list; the lists are short. A failure is logged: the
// list in memory is still right.
void CPageList::Save()
{
    const char* path = reinterpret_cast<const char*>(iPath->Ptr());
    FILE* file = fopen(path, "wb");
    if (!file) {
        BrowserLog("%s: cannot write", path);
        return;
    }
    TRAPD(error, {
        for (TInt i = 0; i < iPages.Count(); ++i)
            WritePageL(file, iPages[i]);
    });
    fclose(file);
    if (error)
        BrowserLog("%s: cannot write (%d)", path, error);
}

void CPageList::WritePageL(FILE* aFile, const TPage& aPage)
{
    HBufC8* url = ToUtf8LC(*aPage.iUrl);
    HBufC8* title = ToUtf8LC(*aPage.iTitle);
    fprintf(aFile, "%s\t%s\n", reinterpret_cast<const char*>(url->Ptr()), reinterpret_cast<const char*>(title->Ptr()));
    CleanupStack::PopAndDestroy(2, url);
}

const TDesC& CPageList::Label(TInt aIndex) const
{
    const TPage& page = iPages[aIndex];
    return page.iTitle->Length() ? *page.iTitle : *page.iUrl;
}

TInt CPageList::Find(const TDesC& aUrl) const
{
    for (TInt i = 0; i < iPages.Count(); ++i) {
        if (*iPages[i].iUrl == aUrl)
            return i;
    }
    return KErrNotFound;
}

void CPageList::AddL(const TDesC& aUrl, const TDesC& aTitle)
{
    if (!aUrl.Length())
        return;
    TPage page;
    page.iUrl = OneLineLC(aUrl);
    page.iTitle = OneLineLC(aTitle);
    TInt existing = Find(*page.iUrl);
    if (existing != KErrNotFound) {
        if (!page.iTitle->Length()) {
            CleanupStack::PopAndDestroy(page.iTitle);
            page.iTitle = iPages[existing].iTitle;
            iPages[existing].iTitle = nullptr;
            CleanupStack::PushL(page.iTitle);
        }
        Drop(existing);
    }
    iPages.InsertL(page, 0);
    CleanupStack::Pop(2, page.iUrl);
    while (iMaxCount > 0 && iPages.Count() > iMaxCount)
        Drop(iPages.Count() - 1);
    Save();
}

void CPageList::SetTitleL(const TDesC& aUrl, const TDesC& aTitle)
{
    TInt index = Find(aUrl);
    if (index == KErrNotFound)
        return;
    HBufC* title = OneLineLC(aTitle);
    if (*title == *iPages[index].iTitle) {
        CleanupStack::PopAndDestroy(title);
        return;
    }
    delete iPages[index].iTitle;
    iPages[index].iTitle = title;
    CleanupStack::Pop(title);
    Save();
}

void CPageList::Remove(TInt aIndex)
{
    Drop(aIndex);
    Save();
}

void CPageList::Reset()
{
    while (iPages.Count())
        Drop(iPages.Count() - 1);
    Save();
}

void CPageList::Drop(TInt aIndex)
{
    delete iPages[aIndex].iUrl;
    delete iPages[aIndex].iTitle;
    iPages.Remove(aIndex);
}
