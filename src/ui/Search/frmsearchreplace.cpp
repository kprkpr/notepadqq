#include "include/Search/frmsearchreplace.h"
#include "include/iconprovider.h"
#include "include/Search/searchinfilesworker.h"
#include "ui_frmsearchreplace.h"
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QFileDialog>
#include <QThread>

frmSearchReplace::frmSearchReplace(TopEditorContainer *topEditorContainer, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::frmSearchReplace),
    m_topEditorContainer(topEditorContainer)
{
    ui->setupUi(this);

    //setFixedSize(this->width(), this->height());
    setWindowFlags( (windowFlags() | Qt::CustomizeWindowHint) & ~Qt::WindowMaximizeButtonHint);

    move(
        parentWidget()->window()->frameGeometry().topLeft() +
        parentWidget()->window()->rect().center() -
        rect().center());

    connect(ui->cmbSearch->lineEdit(), &QLineEdit::textEdited, this, &frmSearchReplace::on_searchStringEdited);
    connect(ui->cmbSearch->lineEdit(), &QLineEdit::returnPressed, this, [=]() {
        if (ui->actionFind_in_files->isChecked()) {
            on_btnFindAll_clicked();
        } else {
            on_btnFindNext_clicked();
        }
    });
    connect(ui->cmbReplace->lineEdit(), &QLineEdit::returnPressed, this, &frmSearchReplace::on_btnFindNext_clicked);
    connect(ui->cmbLookIn->lineEdit(), &QLineEdit::returnPressed, this, &frmSearchReplace::on_btnFindAll_clicked);
    connect(ui->cmbFilter->lineEdit(), &QLineEdit::returnPressed, this, &frmSearchReplace::on_btnFindAll_clicked);

    ui->cmbFilter->lineEdit()->setPlaceholderText("*.ext1, *.ext2, ...");

    ui->actionFind->setIcon(IconProvider::fromTheme("edit-find"));
    ui->actionReplace->setIcon(IconProvider::fromTheme("edit-find-replace"));

    QActionGroup *tabGroup = new QActionGroup(this);
    tabGroup->addAction(ui->actionFind);
    tabGroup->addAction(ui->actionReplace);
    tabGroup->addAction(ui->actionFind_in_files);
    tabGroup->setExclusive(true);

    // Initialize all the tabs
    ui->actionFind->setChecked(true);
    ui->actionReplace->setChecked(true);
    ui->actionFind_in_files->setChecked(true);

    ui->chkShowAdvanced->toggled(ui->chkShowAdvanced->isChecked());

    setCurrentTab(TabSearch);
}

frmSearchReplace::~frmSearchReplace()
{
    delete ui;
}

void frmSearchReplace::keyPressEvent(QKeyEvent *evt)
{
    switch (evt->key())
    {
    case Qt::Key_Escape:
        close();
        break;
    default:
        QMainWindow::keyPressEvent(evt);
    }
}

void frmSearchReplace::show(Tabs defaultTab)
{
    setCurrentTab(defaultTab);
    ui->cmbSearch->setFocus();
    ui->cmbSearch->lineEdit()->selectAll();
    QMainWindow::show();
    manualSizeAdjust();
}

void frmSearchReplace::setCurrentTab(Tabs tab)
{
    if (tab == TabSearch) {
        ui->actionFind->setChecked(true);
    } else if (tab == TabReplace) {
        ui->actionReplace->setChecked(true);
    } else if (tab == TabSearchInFiles) {
        ui->actionFind_in_files->setChecked(true);
    }
}

Editor *frmSearchReplace::currentEditor()
{
    return this->m_topEditorContainer->currentTabWidget()->currentEditor();
}

QString frmSearchReplace::plainTextToRegex(QString text, bool matchWholeWord)
{
    // Transform it into a regex, but make sure to escape special chars
    QString regex = QRegularExpression::escape(text);

    if (matchWholeWord)
        regex = "\\b" + regex + "\\b";

    return regex;
}

QString frmSearchReplace::rawSearchString(QString search, SearchMode searchMode, SearchOptions searchOptions)
{
    QString rawSearch;

    if (searchMode == SearchMode::Regex) {
        rawSearch = search;
    } else if (searchMode == SearchMode::SpecialChars) {
        bool wholeWord = searchOptions.MatchWholeWord;
        rawSearch = plainTextToRegex(search, wholeWord);
        // Replace '\\' with '\' (basically, we unescape escaped slashes)
        rawSearch = rawSearch.replace("\\\\", "\\");
    } else {
        bool wholeWord = searchOptions.MatchWholeWord;
        rawSearch = plainTextToRegex(search, wholeWord);
    }

    return rawSearch;
}

QString frmSearchReplace::regexModifiersFromSearchOptions(SearchOptions searchOptions)
{
    QString modifiers = "m";
    if (!searchOptions.MatchCase)
        modifiers.append("i");

    return modifiers;
}

void frmSearchReplace::search(QString string, SearchMode searchMode, bool forward, SearchOptions searchOptions) {
    if (!string.isEmpty()) {
        QString rawSearch = rawSearchString(string, searchMode, searchOptions);

        Editor *editor = currentEditor();

        if (searchOptions.SearchFromStart) {
            editor->setCursorPosition(0, 0);
        }

        QList<QVariant> data = QList<QVariant>();
        data.append(rawSearch);
        data.append(regexModifiersFromSearchOptions(searchOptions));
        data.append(forward);
        editor->sendMessage("C_FUN_SEARCH", QVariant::fromValue(data));
    }
}

void frmSearchReplace::replace(QString string, QString replacement, SearchMode searchMode, bool forward, SearchOptions searchOptions) {
    if (!string.isEmpty()) {
        QString rawSearch = rawSearchString(string, searchMode, searchOptions);

        Editor *editor = currentEditor();

        if (searchOptions.SearchFromStart) {
            editor->setCursorPosition(0, 0);
        }

        QList<QVariant> data = QList<QVariant>();
        data.append(rawSearch);
        data.append(regexModifiersFromSearchOptions(searchOptions));
        data.append(forward);
        data.append(replacement);
        editor->sendMessage("C_FUN_REPLACE", QVariant::fromValue(data));
    }
}

int frmSearchReplace::replaceAll(QString string, QString replacement, SearchMode searchMode, SearchOptions searchOptions) {
    QString rawSearch = rawSearchString(string, searchMode, searchOptions);

    QList<QVariant> data = QList<QVariant>();
    data.append(rawSearch);
    data.append(regexModifiersFromSearchOptions(searchOptions));
    data.append(replacement);
    QVariant count = currentEditor()->sendMessageWithResult("C_FUN_REPLACE_ALL", QVariant::fromValue(data));
    return count.toInt();
}

int frmSearchReplace::selectAll(QString string, SearchMode searchMode, SearchOptions searchOptions) {
    QString rawSearch = rawSearchString(string, searchMode, searchOptions);

    QList<QVariant> data = QList<QVariant>();
    data.append(rawSearch);
    data.append(regexModifiersFromSearchOptions(searchOptions));
    QVariant count = currentEditor()->sendMessageWithResult("C_FUN_SEARCH_SELECT_ALL", QVariant::fromValue(data));
    return count.toInt();
}

void frmSearchReplace::searchInFiles(QString string, QString path, QStringList filters, SearchMode searchMode, SearchOptions searchOptions)
{
    if (!string.isEmpty()) {
        QMessageBox *msgBox = new QMessageBox(this);
        msgBox->setText(tr("Searching..."));
        msgBox->setWindowTitle(tr("Searching..."));
        msgBox->setStandardButtons(QMessageBox::Cancel);
        msgBox->setGeometry(x(), y(), width(), height()/2);

        QThread *thread = new QThread();
        SearchInFilesWorker *worker = new SearchInFilesWorker(string, path, filters, searchMode, searchOptions);
        worker->moveToThread(thread);
        bool deletingObjects = false;

        connect(thread, &QThread::started, worker, &SearchInFilesWorker::run);

        connect(worker, &SearchInFilesWorker::error, this, [=](QString err){
            if (!deletingObjects)
                msgBox->setText(err);
        });

        connect(worker, &SearchInFilesWorker::progress, this, [=](QString file){
            if (!deletingObjects)
                msgBox->setText(tr("Searching in %1").arg(file));
        });

        connect(worker, &SearchInFilesWorker::finished, this, [=, &deletingObjects](){
            FileSearchResult::SearchResult result = worker->getResult();

            msgBox->hide();

            deletingObjects = true;

            thread->deleteLater();
            worker->deleteLater();
            msgBox->deleteLater();

            emit fileSearchResultFinished(result);
        });

        thread->start();
        msgBox->exec();

        // If we're here, the search finished or the user wants to cancel it.

        if (!deletingObjects) {
            worker->stop();
        }
    }
}

frmSearchReplace::SearchMode frmSearchReplace::searchModeFromUI()
{
    if (ui->radSearchPlainText->isChecked())
        return SearchMode::PlainText;

    else if (ui->radSearchWithSpecialChars->isChecked())
        return SearchMode::SpecialChars;

    else if (ui->radSearchWithRegex->isChecked())
        return SearchMode::Regex;

    else
        return SearchMode::PlainText;
}

frmSearchReplace::SearchOptions frmSearchReplace::searchOptionsFromUI()
{
    SearchOptions searchOptions;

    if (ui->chkMatchCase->isChecked())
        searchOptions.MatchCase = true;
    if (ui->chkMatchWholeWord->isChecked())
        searchOptions.MatchWholeWord = true;
    if (ui->chkIncludeSubdirs->isChecked())
        searchOptions.IncludeSubDirs = true;

    return searchOptions;
}

void frmSearchReplace::findFromUI(bool forward, bool searchFromStart)
{
    SearchOptions sOpts = searchOptionsFromUI();
    sOpts.SearchFromStart = searchFromStart;

    this->search(ui->cmbSearch->currentText(),
                 searchModeFromUI(),
                 forward,
                 sOpts);
}

void frmSearchReplace::replaceFromUI(bool forward, bool searchFromStart)
{
    SearchOptions sOpts = searchOptionsFromUI();
    sOpts.SearchFromStart = searchFromStart;

    this->replace(ui->cmbSearch->currentText(),
                  ui->cmbReplace->currentText(),
                  searchModeFromUI(),
                  forward,
                  sOpts);
}

void frmSearchReplace::on_btnFindNext_clicked()
{
    findFromUI(true);
}

void frmSearchReplace::on_btnFindPrev_clicked()
{
    findFromUI(false);
}

void frmSearchReplace::on_btnReplaceNext_clicked()
{
    replaceFromUI(true);
}

void frmSearchReplace::on_btnReplacePrev_clicked()
{
    replaceFromUI(false);
}

void frmSearchReplace::on_btnReplaceAll_clicked()
{
    int n = this->replaceAll(ui->cmbSearch->currentText(),
                             ui->cmbReplace->currentText(),
                             searchModeFromUI(),
                             searchOptionsFromUI());
    QMessageBox::information(this, tr("Replace all"), tr("%1 occurrences have been replaced.").arg(n));
}

void frmSearchReplace::on_btnSelectAll_clicked()
{
    int count = this->selectAll(ui->cmbSearch->currentText(),
                                searchModeFromUI(),
                                searchOptionsFromUI());
    if (count == 0) {
        QMessageBox::information(this, tr("Select all"), tr("No results found"));
    } else {
        // Focus on main window
        this->m_topEditorContainer->activateWindow();
    }
}

void frmSearchReplace::on_actionReplace_toggled(bool on)
{
    ui->btnReplaceAll->setVisible(on);
    ui->btnReplaceNext->setVisible(on);
    ui->btnReplacePrev->setVisible(on);
    ui->cmbReplace->setVisible(on);
    ui->lblReplace->setVisible(on);

    ui->cmbSearch->setFocus();

    manualSizeAdjust();
}

void frmSearchReplace::on_actionFind_toggled(bool /*on*/)
{
    ui->cmbSearch->setFocus();

    manualSizeAdjust();
}

void frmSearchReplace::on_actionFind_in_files_toggled(bool on)
{
    ui->lblLookIn->setVisible(on);
    ui->cmbLookIn->setVisible(on);
    ui->lblFilter->setVisible(on);
    ui->cmbFilter->setVisible(on);
    ui->btnLookInBrowse->setVisible(on);
    ui->btnFindAll->setVisible(on);
    ui->lblSpacer1->setVisible(on);
    ui->lblSpacer2->setVisible(on);
    ui->chkIncludeSubdirs->setVisible(on);
    ui->btnFindNext->setVisible(!on);
    ui->btnFindPrev->setVisible(!on);
    ui->btnSelectAll->setVisible(!on);

    ui->cmbSearch->setFocus();

    manualSizeAdjust();
}

void frmSearchReplace::manualSizeAdjust()
{
    int curX = geometry().x();
    int curY = geometry().y();

    QApplication::processEvents();
    QApplication::processEvents();
    setGeometry(curX, curY, width(), 0);

    setFixedSize(width(), height());
}

void frmSearchReplace::on_chkShowAdvanced_toggled(bool checked)
{
    if (checked)
        ui->groupAdvanced->show();
    else
        ui->groupAdvanced->hide();

    manualSizeAdjust();
}

void frmSearchReplace::on_radSearchWithRegex_toggled(bool checked)
{
    if (checked) {
        ui->chkMatchWholeWord->setChecked(false);
        ui->chkMatchWholeWord->setEnabled(false);

        manualSizeAdjust();
    }
}

void frmSearchReplace::on_radSearchPlainText_toggled(bool checked)
{
    if (checked) {
        ui->chkMatchWholeWord->setEnabled(true);

        manualSizeAdjust();
    }
}

void frmSearchReplace::on_radSearchWithSpecialChars_toggled(bool checked)
{
    if (checked) {
        ui->chkMatchWholeWord->setChecked(false);
        ui->chkMatchWholeWord->setEnabled(false);

        manualSizeAdjust();
    }
}

void frmSearchReplace::on_searchStringEdited(const QString &/*text*/)
{
    QSettings s;

    if (s.value("Search/SearchAsIType", true).toBool()) {
        if (ui->actionFind->isChecked()) {
            Editor *editor = currentEditor();

            QList<Editor::Selection> selections = editor->selections();
            if (selections.length() > 0) {
                editor->setCursorPosition(
                            std::min(selections[0].from, selections[0].to));
            }

            findFromUI(true);
        }
    }
}

void frmSearchReplace::on_btnFindAll_clicked()
{
    searchInFiles(ui->cmbSearch->currentText(),
                  ui->cmbLookIn->currentText(),
                  ui->cmbFilter->currentText().split(",", QString::SkipEmptyParts),
                  searchModeFromUI(),
                  searchOptionsFromUI());
}

void frmSearchReplace::on_btnLookInBrowse_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Look in"),
                                                     ui->cmbLookIn->currentText(),
                                                     QFileDialog::ShowDirsOnly
                                                     | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        ui->cmbLookIn->setCurrentText(dir);
    }
}