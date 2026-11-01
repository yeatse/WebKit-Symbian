#include <qt/symbian_input_dialog.h>
#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QMetaObject>
#include <iostream>
#include <memory>

int checks = 0;
int failures = 0;
void check(bool value, const char *name) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    const QString ordinary = QString::fromUtf8("fixture 中文 🙂\nsecond line");
    const QString secret = QString::fromUtf8("fixture-secret-中文-🙂");
    auto dialog = std::make_unique<symbian_input_dialog>();
    auto *plain = dialog->findChild<QPlainTextEdit *>("plainTextEdit");
    auto *masked = dialog->findChild<QLineEdit *>("secretLineEdit");
    check(plain && masked, "actual editor children");
    if (!plain || !masked) return 1;
    int completions = 0;
    bool cancelled = false;
    QString submitted;
    QObject::connect(dialog.get(), &symbian_input_dialog::finished_input,
        [&](const QString &text, bool force_close) { ++completions; submitted = text; cancelled = force_close; });
    dialog->open_for_input(ordinary, 256, false);
    check(plain->isVisible() && !masked->isVisible(), "ordinary editor selection");
    check(plain->toPlainText() == ordinary, "ordinary Unicode multiline preserved");
    check(dialog->get_current_text() == ordinary, "ordinary returned text");
    check(masked->text().isEmpty(), "hidden secret editor empty on ordinary request");
    QMetaObject::invokeMethod(dialog.get(), "event_submit_pressed", Qt::DirectConnection);
    check(completions == 1 && !cancelled && submitted == ordinary, "ordinary Submit callback");
    dialog->open_for_input(secret, 256, true);
    check(masked->isVisible() && !plain->isVisible(), "secret editor selection");
    check(masked->echoMode() == QLineEdit::Password, "Password mode before text display");
    check(masked->displayText() != secret, "secret visible text masked");
    check(masked->displayText().size() == secret.size(), "mask covers all UTF16 units");
    check(plain->toPlainText().isEmpty(), "secret never enters plaintext document");
    check(dialog->get_current_text() == secret, "secret logical value preserved");
    check((masked->inputMethodHints() & Qt::ImhSensitiveData) != 0, "sensitive input hint");
    check((masked->inputMethodHints() & Qt::ImhHiddenText) != 0, "hidden input hint");
    QMetaObject::invokeMethod(dialog.get(), "event_submit_pressed", Qt::DirectConnection);
    check(completions == 2 && !cancelled && submitted == secret, "secret Submit callback");
    dialog->open_for_input(QString{}, 256, true);
    QMetaObject::invokeMethod(dialog.get(), "event_submit_pressed", Qt::DirectConnection);
    check(completions == 3 && !cancelled && submitted.isEmpty(), "accepted empty secret");
    dialog->open_for_input(secret, 4, true);
    check(masked->maxLength() == 4 && masked->text().size() == 4, "secret maximum length");
    check(plain->toPlainText().isEmpty(), "secret limit has no plaintext copy");
    dialog->open_for_input(ordinary, 256, false);
    check(masked->text().isEmpty() && !masked->isVisible(), "secret cleared when returning to ordinary");
    check(plain->toPlainText() == ordinary && plain->isVisible(), "ordinary mode restored");
    dialog->open_for_input(secret, 256, true);
    dialog->reject();
    check(completions == 4 && cancelled && submitted.isEmpty(), "Cancel emits no draft commit");
    dialog.reset();
    std::cout << "Qt offscreen widget checks " << checks << ", failures " << failures << '\n';
    return failures ? 1 : 0;
}
