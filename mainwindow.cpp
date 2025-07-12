#include "mainwindow.h"

#include <QProcess>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QSettings>
#include <QFileDialog>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QAction>
#include <QVariant>
#include <QProgressDialog>

#include "MyCppDifferent.h"
#include "MyQShortings.h"
#include "MyQDifferent.h"
#include "MyQFileDir.h"
#include "MyQDialogs.h"
#include "MyQExecute.h"
#include "MyQTimer.h"
#include "MyQTableWidget.h"

#include "git.h"

namespace ColIndexes {
	const int directory			= 0;
	const int commitStatus		= directory+1;
	const int pushStatus		= commitStatus+1;
	const int remoteRepos		= pushStatus+1;
	const int errorOutput		= remoteRepos+1;
	const int standartOutput	= errorOutput+1;
	const int remoteOutput		= standartOutput+1;

	const int colCount = remoteOutput + 1;
}

namespace Colors {
	const QColor green(146,208,80);

}

void ReplaceInTextEdit(QTextEdit *textEdit)
{
	auto text = textEdit->toPlainText();
	text.replace('\\','/');
	textEdit->setPlainText(text);
}

namespace forCommitsParser {
	MainWindow *mainWindowPtr;
}
QString MainWindow::ReadAndGetGitExtensionsExe(QString dir, bool showInfoMessageBox)
{
	if(GitExtensionsExe.isEmpty() && QFile::exists(filesPath+"/git_extensions_exe.txt"))
		GitExtensionsExe = MyQFileDir::ReadFile1(filesPath+"/git_extensions_exe.txt");

	if(GitExtensionsExe.isEmpty() || !QFileInfo(GitExtensionsExe).isFile())
	{
		if(showInfoMessageBox) QMbInfo("Укажите программу для работы с репозиториями");
		GitExtensionsExe = QFileDialog::getOpenFileName(nullptr, "Укажите программу для работы с репозиториями",
														dir, "*.exe");
		MyQFileDir::WriteFile(filesPath+"/git_extensions_exe.txt", GitExtensionsExe);
	}
	return GitExtensionsExe;
}

MainWindow::MainWindow(QWidget *parent)
	: QWidget(parent)
{
	if(!QDir().mkpath(filesPath)) QMbError("cant create path " + filesPath);

	auto basicFont = QFont("Courier new",12);

	auto layOutMain = new QVBoxLayout(this);
	auto layOutTop = new QHBoxLayout();
	splitterCenter = new QSplitter(Qt::Horizontal, this);
	layOutMain->addLayout(layOutTop);
	layOutMain->addWidget(splitterCenter);

	// left part
	auto widgetLeft = new QWidget(this);
	auto widgetRight = new QWidget(this);
	splitterCenter->addWidget(widgetLeft);
	splitterCenter->addWidget(widgetRight);
	auto loLeft = new QVBoxLayout(widgetLeft);
	auto loRight = new QVBoxLayout(widgetRight);

	loLeft->addWidget(new QLabel("Сканируемые пути",this));
	textEdit = new QTextEdit(this);
	loLeft->addWidget(textEdit);
	textEdit->setFont(basicFont);
	auto btn = new QPushButton("Сканировать локально", this);
	loLeft->addWidget(btn);
	connect(btn,&QPushButton::clicked,this,&MainWindow::SlotScan);
	btn = new QPushButton("Обновить удаленные", this);
	loLeft->addWidget(btn);
	connect(btn,&QPushButton::clicked,this,&MainWindow::SlotCheckRemotes);
	btn = new QPushButton(" Сканировать и обновить ", this);
	loLeft->addWidget(btn);
	connect(btn,&QPushButton::clicked, this, &MainWindow::SlotScanAndCheckRemotes);
	btn = new QPushButton(" Сканировать и обновить отображаемые ", this);
	loLeft->addWidget(btn);
	connect(btn,&QPushButton::clicked, this, &MainWindow::SlotScanAndCheckRemotesCurrent);


	chBoxStopAtCount = new QCheckBox("Остановиться на:");
	leCountToStopAt = new QLineEdit;
#ifdef QT_DEBUG
	auto hloDebugStopper = new QHBoxLayout;
	loLeft->addLayout(hloDebugStopper);
	hloDebugStopper->addWidget(chBoxStopAtCount);
	hloDebugStopper->addWidget(leCountToStopAt);
#endif

	// right part
	// header buttons
	auto loRihgtHeader = new QHBoxLayout;
	loRight->addLayout(loRihgtHeader);
	btn = new QPushButton("Показать все", this);
	loRihgtHeader->addWidget(btn);
	connect(btn,&QPushButton::clicked,[this](){
		for(int i=0; i<tableWidget->rowCount(); i++)
			tableWidget->showRow(i);
	});
	btn = new QPushButton("Скрыть "+GitStatus::notGit, this);
	loRihgtHeader->addWidget(btn);
	connect(btn,&QPushButton::clicked, this, &MainWindow::SlotHideNotGit);

	btn = new QPushButton("Скрыть " + Statuses::commited() + ", " + Statuses::pushed() + ", remote ok", this);
	loRihgtHeader->addWidget(btn);
	connect(btn,&QPushButton::clicked, this, &MainWindow::SlotHideCommitedPushedRemoteOk);

	// table
	tableWidget = new QTableWidget(this);
	loRight->addWidget(tableWidget);
	tableWidget->setFont(basicFont);
	tableWidget->setColumnCount(ColIndexes::colCount);
	tableWidget->hideColumn(ColIndexes::errorOutput);
	tableWidget->hideColumn(ColIndexes::standartOutput);
	tableWidget->hideColumn(ColIndexes::remoteOutput);
	tableWidget->verticalHeader()->hide();
	tableWidget->horizontalHeader()->setStyleSheet("QHeaderView { border-style: none; border-bottom: 1px solid gray; }");
	tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

	connect(tableWidget, &QTableWidget::cellDoubleClicked, [this](int row, int){
		QString text = "Output text:\n";
		if(auto item = tableWidget->item(row, ColIndexes::standartOutput)) text += item->text();
		text += "\n\nErrors text:\n";
		if(auto item = tableWidget->item(row, ColIndexes::errorOutput)) text += item->text();
		text += "\n\nRemote repos text:\n";
		if(auto item = tableWidget->item(row, ColIndexes::remoteOutput)) text += item->text();
		MyQDialogs::ShowText(text,600,300);
	});

	CreateContextMenu();

	LoadSettings();
}

void MainWindow::LaunchAndStartScan()
{
	MainWindow *w = new MainWindow;
	w->show();
	QTimer::singleShot(300,[w](){
		w->SlotScanAndCheckRemotes();
	});
}

void MainWindow::CreateContextMenu()
{
	tableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);

	QAction *mShowInExplorer = new QAction("Открыть каталог", tableWidget);
	QAction *mUpdateLocal = new QAction("Обновить статус локальный", tableWidget);
	QAction *mUpdateRemote = new QAction("Обновить статус удалённых", tableWidget);
	QAction *mUpdateLocalAndRemote = new QAction("Обновить статус локальный и удалённых", tableWidget);

	QAction *mOpenRepo = new QAction("GitExt Open repository", tableWidget);
	QAction *mCommit = new QAction("GitExt Commit", tableWidget);
	QAction *mPush = new QAction("GitExt Push", tableWidget);
	QAction *mSetGitExtensions = new QAction("Указать программу для работы с репозиториями", tableWidget);

	tableWidget->addAction(mShowInExplorer);

	tableWidget->addAction(new QAction(tableWidget));
	tableWidget->actions().back()->setSeparator(true);

	tableWidget->addAction(mUpdateLocal);
	tableWidget->addAction(mUpdateRemote);
	tableWidget->addAction(mUpdateLocalAndRemote);

	tableWidget->addAction(new QAction(tableWidget));
	tableWidget->actions().back()->setSeparator(true);

	tableWidget->addAction(mCommit);
	tableWidget->addAction(mPush);

	tableWidget->addAction(new QAction(tableWidget));
	tableWidget->actions().back()->setSeparator(true);

	tableWidget->addAction(mOpenRepo);

	tableWidget->addAction(new QAction(tableWidget));
	tableWidget->actions().back()->setSeparator(true);

	tableWidget->addAction(mSetGitExtensions);

	auto GetSelectedRowsAndAsk = [this]() -> std::set<int> {
		auto rows = MyQTableWidget::SelectedRows(tableWidget, true);
		if(rows.size() > 7)
		{
			auto answ = QMessageBox::question({}, "", "Selected " + QSn(rows.size()) + " rows, the operation may take a long time. Continue?");
			if(answ == QMessageBox::No) return {};
		}
		return rows;
	};

	connect(mShowInExplorer, &QAction::triggered,[this, GetSelectedRowsAndAsk](){
		auto rows = GetSelectedRowsAndAsk();
		for(auto &row:rows)
			MyQExecute::OpenDir(tableWidget->item(row,0)->text());
	});

	connect(mUpdateLocal, &QAction::triggered, [this, GetSelectedRowsAndAsk](){
		auto rows = GetSelectedRowsAndAsk();
		for(auto &row:rows)
			UpdateLocal(row);
	});

	connect(mUpdateRemote, &QAction::triggered, [this, GetSelectedRowsAndAsk](){
		auto rows = GetSelectedRowsAndAsk();
		for(auto &row:rows)
			UpdateRemote(row);
	});

	connect(mUpdateLocalAndRemote, &QAction::triggered, [this, GetSelectedRowsAndAsk](){
		auto rows = GetSelectedRowsAndAsk();
		for(auto &row:rows)
		{
			UpdateLocal(row);
			UpdateRemote(row);
		}
	});

	connect(mOpenRepo, &QAction::triggered,[this, GetSelectedRowsAndAsk](){
		auto rows = GetSelectedRowsAndAsk();
		for(auto &row:rows)
		{
			QString dir = tableWidget->item(row,ColIndexes::directory)->text();
			MyQExecute::Execute(ReadAndGetGitExtensionsExe(GitExtensionsExe, true), {dir});
		}
	});

	connect(mCommit, &QAction::triggered,[this, GetSelectedRowsAndAsk](){
		auto rows = GetSelectedRowsAndAsk();
		for(auto &row:rows)
		{
			QString dir = tableWidget->item(row,ColIndexes::directory)->text();
			MyQExecute::Execute(ReadAndGetGitExtensionsExe(GitExtensionsExe, true), {"commit", dir});
		}
	});

	connect(mPush, &QAction::triggered,[this, GetSelectedRowsAndAsk](){
		auto rows = GetSelectedRowsAndAsk();
		for(auto &row:rows)
		{
			QString dir = tableWidget->item(row,ColIndexes::directory)->text();
			MyQExecute::Execute(ReadAndGetGitExtensionsExe(GitExtensionsExe, true), {"push", dir});
		}
	});

	connect(mSetGitExtensions, &QAction::triggered,[this](){
		QString prevGitExtesions;
		prevGitExtesions = GitExtensionsExe;
		GitExtensionsExe.clear();

		ReadAndGetGitExtensionsExe(prevGitExtesions, false);

		if(GitExtensionsExe.isEmpty()) GitExtensionsExe = prevGitExtesions;
	});
}



void MainWindow::UpdateLocal(int row)
{
	QString dir = tableWidget->item(row,ColIndexes::directory)->text();
	QProcess process;
	process.setWorkingDirectory(dir);
	auto gitStatus = Git::GetGitStatusForOneDir(process, dir);
	SetRow(row,gitStatus);
}

MainWindow::UpdateRemoteRes MainWindow::UpdateRemote(int row)
{
	tableWidget->item(row,ColIndexes::remoteRepos)->setBackground(QColor(255,255,255));

	QString dir = tableWidget->item(row,ColIndexes::directory)->text();
	QStringList remotes = tableWidget->item(row,ColIndexes::remoteRepos)->text().split(' ',QString::SkipEmptyParts);
	std::vector<int> updated;
	const int undefined = 1;
	const int fetchEmpty = 1;
	const int fetchAndDiffEmpty = 3;
	QProcess process;
	process.setWorkingDirectory(dir);

	QString result;
	int countTries = 3;
	bool stopAll = false;
	for(auto &remote:remotes)
	{
		int &curUpdated = updated.emplace_back(undefined);
		result += "\n\nstart work with " + remote + "\n";
		bool stop = false;
		bool did = false;
		while(!stop && !did)
		{
			result += "git fetch " + remote + "\n";
			QString errors;
			for(int i=0; i<countTries; i++)
			{
				auto fetchRes = Git::DoGitCommand(process, QStringList() << "fetch" << remote);
				if(fetchRes.success && fetchRes.errorOutput.isEmpty())
				{
					result += "fetch success\n";
					result += fetchRes.standartOutput;
					if(fetchRes.standartOutput.isEmpty()) { result += "fetch empty result\n"; curUpdated = fetchEmpty; }
					did = true;
					break;
				}
				else
				{
					if(!fetchRes.error.isEmpty()) errors += "fetch error:\n" + fetchRes.error + "\n";
					if(!fetchRes.errorOutput.isEmpty()) errors += "fetch errorOutput:\n" + fetchRes.errorOutput + "\n";
					MyCppDifferent::sleep_ms(10);
				}
			}

			if(!did)
			{
				auto res = MyQDialogs::CustomDialog("Git fetch не выполнено",
													"Git fetch не выполнено (3 попытки); ошибки:\n" + errors,
													{"Повторить", "Прервать"});
				if(res == "Повторить") {}
				else if(res == "Прервать") { stop = true; stopAll = true; }
				else QMbc(0,"","wrong answ [" + res + "]");
			}
			if(stop) break;
		}
		if(stopAll) break;

		stop = did = false;
		while(!stop && !did)
		{
			result += "git diff --name-status master "+remote+"/master\n";
			QString errors;
			for(int i=0; i<countTries; i++)
			{
				auto difRes = Git::DoGitCommand(process, QStringList() << "diff" << "--name-status" << "master" <<  remote+"/master");
				if(difRes.success && difRes.errorOutput.isEmpty())
				{
					result += difRes.standartOutput;
					if(difRes.standartOutput.isEmpty()) { result += "diff empty result\n"; if(curUpdated == fetchEmpty)
							curUpdated = fetchAndDiffEmpty; }
					did = true;
					break;
				}
				else
				{
					if(!difRes.error.isEmpty()) errors += "diff error:\n" + difRes.error + "\n";
					if(!difRes.errorOutput.isEmpty()) errors += "diff errorOutput:\n" + difRes.errorOutput + "\n";
					MyCppDifferent::sleep_ms(10);
				}
			}

			if(!did)
			{
				auto res = MyQDialogs::CustomDialog("Git diff не выполнено",
													"Git diff не выполнено (3 попытки); ошибки:\n" + errors,
													{"Повторить", "Прервать"});
				if(res == "Повторить") {}
				else if(res == "Прервать") { stop = true; stopAll = true; }
				else QMbc(0,"","wrong answ [" + res + "]");
			}
			if(stop) { stop = false; break; }
		}
		if(stopAll) break;
	}

	tableWidget->item(row,ColIndexes::remoteOutput)->setText(result);

	QColor color(Colors::green);
	for(auto &update:updated) if(update != fetchAndDiffEmpty) { color = {208,146,80}; break; }
	tableWidget->item(row,ColIndexes::remoteRepos)->setBackground(color);
	tableWidget->setCurrentCell(row,0);

	if(stopAll) return stopAllChecks;
	else return ok;
}

QStringList GetAllNestedDirs(QString path, QDir::SortFlag sort = QDir::NoSort)
{
	QDir dir(path);
	QStringList res;
	QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, sort);
	if(!subdirs.contains(GitStatus::gitRepoDir()))
	{
		for (const auto &subdir : subdirs) {
			res << path + "/" + subdir;
			res += GetAllNestedDirs(path + "/" + subdir);
		}
	}
	return res;

	/// this realisation differs from MyQFileDir by checking .git subdir and ignoring all subdirs if contains
	/// check subdirs.contains(...) is prefere than QFileInfo(...).isDir() because if not contains we anyway will get entryList
}

void MainWindow::SlotScan()
{
	ReplaceInTextEdit(textEdit);

	tableWidget->clearContents();
	auto dirs = textEdit->toPlainText().split('\n');
	QStringList allDirs;
	QString badDirs;
	for(auto &dir:dirs)
	{
		if(QDir(dir).exists()) allDirs += GetAllNestedDirs(dir);
		else badDirs += "\n" + dir;
	}
	if(badDirs.size())
		QMbw(this,"Ошибка","Несуществующие директории:"+badDirs);

	if(chBoxStopAtCount->isChecked())
		while(allDirs.size() > (int)leCountToStopAt->text().toUInt()) allDirs.removeLast();

	tableWidget->setRowCount(1);
	tableWidget->showRow(0);
	tableWidget->setItem(0,0,new QTableWidgetItem);
	forCommitsParser::mainWindowPtr = this;
	auto progress = [](QString progress)
	{
		forCommitsParser::mainWindowPtr->tableWidget->item(0,0)->setText(progress);
		forCommitsParser::mainWindowPtr->tableWidget->repaint();
		QCoreApplication::processEvents();
	};
	auto result = Git::GetGitStatus(allDirs,progress);
	tableWidget->clearContents();
	tableWidget->setRowCount(result.size());

	for(int i=0; i<(int)result.size(); i++)
	{
		SetRow(i, result[i]);
	}
}

void MainWindow::SlotCheckRemotes()
{
	std::vector<int> validRows;
	for(int row=0; row<tableWidget->rowCount(); row++)
		if(tableWidget->item(row,ColIndexes::commitStatus)->text() != GitStatus::notGit)
			validRows.push_back(row);

	QProgressDialog progressDialog("Updating...", {}, 0, validRows.size(), this);
	int i = 0;
	for(auto &row:validRows)
	{
		progressDialog.setValue(i++);
		QCoreApplication::processEvents();

		UpdateLocal(row);

		auto updateRemoteRes = UpdateRemote(row);
		if(updateRemoteRes == stopAllChecks)
		{
			auto res = MyQDialogs::CustomDialog("Ошибка", "Не удалось обновить удаленный репозиторий",
												{"Перейти к следующему", "Прервать"});
			if(res == "Перейти к следующему") { updateRemoteRes = ok; }
			else if(res == "Прервать") { }
			else QMbc(0,"","wrong answ [" + res + "]");
		}
	}
}

void MainWindow::SlotScanAndCheckRemotes()
{
	SlotScan();
	SlotHideNotGit();
	SlotCheckRemotes();
	SlotHideCommitedPushedRemoteOk();
}

void MainWindow::SlotScanAndCheckRemotesCurrent()
{
	std::vector<int> visibleRows;
	for(int row=0; row<tableWidget->rowCount(); row++) if(!tableWidget->isRowHidden(row)) visibleRows.push_back(row);

	QProgressDialog progressDialog("Updating...", "Cancel", 0, visibleRows.size(), this);
	int i = 0;
	for(auto &row:visibleRows)
	{
		progressDialog.setValue(i++);
		if(progressDialog.wasCanceled()) break;
		QCoreApplication::processEvents();

		UpdateLocal(row);
		UpdateRemote(row);
	}
}

void MainWindow::SlotHideNotGit()
{
	for(int i=0; i<tableWidget->rowCount(); i++)
		if(tableWidget->item(i,ColIndexes::commitStatus)->text().contains(GitStatus::notGit))
			tableWidget->hideRow(i);
}

void MainWindow::SlotHideCommitedPushedRemoteOk()
{
	bool hasVisibleRow = false;
	for(int row=0; row<tableWidget->rowCount(); row++)
	{
		if(tableWidget->item(row,ColIndexes::commitStatus)->text() == Statuses::commited()
				&& tableWidget->item(row,ColIndexes::pushStatus)->text() == Statuses::pushed()
				&& tableWidget->item(row,ColIndexes::remoteRepos)->backgroundColor() == Colors::green)
			tableWidget->hideRow(row);

		if(!tableWidget->isRowHidden(row)) hasVisibleRow = true;
	}

	if(!hasVisibleRow)
	{
		tableWidget->setRowCount(tableWidget->rowCount()+1);
		tableWidget->setItem(tableWidget->rowCount()-1,0,new QTableWidgetItem("All repos updated"));
	}
}

void MainWindow::closeEvent(QCloseEvent * event)
{
	ReplaceInTextEdit(textEdit);

	QSettings settings(filesPath + "/settings.ini", QSettings::IniFormat);
	settings.setValue("geo",saveGeometry());
	settings.setValue("splitterCenter",splitterCenter->saveState());
	settings.setValue("textEdit",textEdit->toPlainText());
	settings.setValue("GitExtensionsExe", GitExtensionsExe);
	settings.setValue("leCountToStopAt", leCountToStopAt->text());
	settings.beginGroup("table");
	for(int i=0; i<tableWidget->columnCount(); i++)
		settings.setValue("col"+QSn(i),tableWidget->columnWidth(i));

	event->accept();
}

void MainWindow::LoadSettings()
{
	QSettings settings(filesPath + "/settings.ini", QSettings::IniFormat);
	this->restoreGeometry(settings.value("geo").toByteArray());
	splitterCenter->restoreState(settings.value("splitterCenter").toByteArray());
	textEdit->setPlainText(settings.value("textEdit").toString());
	GitExtensionsExe = settings.value("GitExtensionsExe").toString();
	leCountToStopAt->setText(settings.value("leCountToStopAt").toString());
	settings.beginGroup("table");
	for(int i=0; i<tableWidget->columnCount(); i++)

void MainWindow::CreateRow(int row)
{
	tableWidget->setItem(row, ColIndexes::directory,		new QTableWidgetItem());
	//tableWidget->setCellWidget(row, ColIndexes::buttons,	CreateButtonsInRow(row)); // вызывается в SetRow только для репозиториев
	tableWidget->setItem(row, ColIndexes::commitStatus,		new QTableWidgetItem());
	tableWidget->setItem(row, ColIndexes::pushStatus,		new QTableWidgetItem());

	tableWidget->setItem(row, ColIndexes::remoteRepos		, new QTableWidgetItem());
	tableWidget->setItem(row, ColIndexes::errorOutput		, new QTableWidgetItem());
	tableWidget->setItem(row, ColIndexes::standartOutput	, new QTableWidgetItem());
	tableWidget->setItem(row, ColIndexes::remoteOutput		, new QTableWidgetItem);
}

QWidget *MainWindow::CreateButtonsInRow(int row)
{
	QWidget *widget = new QWidget;
	auto hlo = new QHBoxLayout(widget);
	hlo->setContentsMargins(0,0,0,0);
	hlo->setSpacing(0);

	auto btnUpdate = new QToolButton();
	btnUpdate->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
	btnUpdate->setToolTip("Update");
	hlo->addWidget(btnUpdate);

	auto btnCommit = new QToolButton();
	btnCommit->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton));
	btnUpdate->setToolTip("Commit");
	hlo->addWidget(btnCommit);

	auto btnPush = new QToolButton();
	btnPush->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
	btnUpdate->setToolTip("Push");
	hlo->addWidget(btnPush);

	auto btnOpenRepo = new QToolButton();
	btnOpenRepo->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogHelpButton));
	btnUpdate->setToolTip("Open repository");
	hlo->addWidget(btnOpenRepo);

	connect(btnUpdate, &QToolButton::clicked, this, [this, row](){ UpdateLocal(row); UpdateRemote(row); });
	connect(btnCommit, &QToolButton::clicked, this, [this, row](){
		QString dir = tableWidget->item(row,ColIndexes::directory)->text();
		MyQExecute::Execute(ReadAndGetGitExtensionsExe(GitExtensionsExe, true), {"commit", dir});	});
	connect(btnPush, &QToolButton::clicked, this, [this, row](){
		QString dir = tableWidget->item(row,ColIndexes::directory)->text();
		MyQExecute::Execute(ReadAndGetGitExtensionsExe(GitExtensionsExe, true), {"push", dir});		});
	connect(btnOpenRepo, &QToolButton::clicked, this, [this, row](){
		QString dir = tableWidget->item(row,ColIndexes::directory)->text();
		MyQExecute::Execute(ReadAndGetGitExtensionsExe(GitExtensionsExe, true), {dir});				});

	return widget;
}

void MainWindow::SetRow(int row, const GitStatus &gitStatusResult)
{
	if(gitStatusResult.commitStatus != GitStatus::notGit && !tableWidget->cellWidget(row, ColIndexes::buttons))
		tableWidget->setCellWidget(row, ColIndexes::buttons, CreateButtonsInRow(row));

	tableWidget->item(row, ColIndexes::directory)->setText(gitStatusResult.dir);
	if(gitStatusResult.error.size())
		tableWidget->item(row, ColIndexes::commitStatus)->setText(gitStatusResult.error);
	else
	{
		tableWidget->item(row, ColIndexes::commitStatus)->setText(gitStatusResult.commitStatus);
		tableWidget->item(row, ColIndexes::pushStatus)->setText(gitStatusResult.pushStatus);
	}
	tableWidget->item(row, ColIndexes::remoteRepos)->setText(gitStatusResult.RemoteRepoNames());
	tableWidget->item(row, ColIndexes::errorOutput)->setText(gitStatusResult.errorOutput);
	tableWidget->item(row, ColIndexes::standartOutput)->setText(gitStatusResult.standartOutput);

	if(tableWidget->item(row,ColIndexes::commitStatus)->text() == Statuses::commited()
			&& tableWidget->item(row,ColIndexes::pushStatus)->text() == Statuses::pushed())
	{
		tableWidget->item(row,ColIndexes::directory)->setBackground(Colors::green);
		tableWidget->item(row,ColIndexes::commitStatus)->setBackground(Colors::green);
		tableWidget->item(row,ColIndexes::pushStatus)->setBackground(Colors::green);
	}
}


