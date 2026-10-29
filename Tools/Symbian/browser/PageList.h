// A list of pages (address and title) kept in a file, for the history and
// the bookmarks. The file has a line per page, "address<TAB>title", in UTF-8.
#pragma once

#include <e32base.h>
#include <stdio.h>

// UTF-16 to UTF-8, zero-terminated, for the engine, the log and the files.
HBufC8* ToUtf8LC(const TDesC& aText);

class CPageList : public CBase {
public:
    // Reads aPath if it exists. A list with aMaxCount > 0 drops its oldest
    // pages beyond that many.
    static CPageList* NewL(const char* aPath, TInt aMaxCount);
    ~CPageList();

    TInt Count() const { return iPages.Count(); }
    const TDesC& Url(TInt aIndex) const { return *iPages[aIndex].iUrl; }
    // The title, or the address for a page without one.
    const TDesC& Label(TInt aIndex) const;
    TInt Find(const TDesC& aUrl) const;

    // Puts the page first, replacing any entry with the same address. An
    // empty title keeps the one the entry had.
    void AddL(const TDesC& aUrl, const TDesC& aTitle);
    // Sets the title of the page with that address, if it is in the list.
    void SetTitleL(const TDesC& aUrl, const TDesC& aTitle);
    void Remove(TInt aIndex);
    void Reset();

private:
    // HBufCs come from the user heap: they are freed with User::Free, as
    // delete would hand them to the SDK runtime's allocator.
    struct TPage {
        HBufC* iUrl;
        HBufC* iTitle;
    };

    CPageList(TInt aMaxCount);
    void ConstructL(const char* aPath);
    void ReadL();
    void Save();
    static void WritePageL(FILE* aFile, const TPage& aPage);
    // Removes a page without saving the list.
    void Drop(TInt aIndex);

    RArray<TPage> iPages;
    HBufC8* iPath { nullptr };
    TInt iMaxCount;
};
