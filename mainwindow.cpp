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

#include "MyCppDifferent.h"
#include "MyQShortings.h"
#include "MyQDifferent.h"
#include "MyQFileDir.h"
#include "MyQDialogs.h"
#include "MyQExecute.h"
#include "MyQTimer.h"

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
	if(GitExtensionsExe.isEmpty() || !QFileInfo(GitExtensionsExe).isFile())
	{
		if(showInfoMessageBox) QMbInfo("Укажите программу для работы с репозиториями");
		GitExtensionsExe = QFileDialog::getOpenFileName(nullptr, "Укажите программу для работы с репозиториями",
														dir, "*.exe");
	}
	return GitExtensionsExe;
}

MainWindow::MainWindow(QWidget *parent)
	: QWidget(parent)
{
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
	connect(btn,&QPushButton::clicked,[this](){
		SlotScan();
		SlotHideNotGit();
		SlotCheckRemotes();
	});

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
	btn = new QPushButton("Скрыть " + Statuses::commited + " " + Statuses::pushed, this);
	loRihgtHeader->addWidget(btn);
	connect(btn,&QPushButton::clicked, this, &MainWindow::SlotHideCommitedPushed);

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

	tableWidget->addAction(new QAction);
	tableWidget->actions().back()->setSeparator(true);

	tableWidget->addAction(mUpdateLocal);
	tableWidget->addAction(mUpdateRemote);
	tableWidget->addAction(mUpdateLocalAndRemote);

	tableWidget->addAction(new QAction);
	tableWidget->actions().back()->setSeparator(true);

	tableWidget->addAction(mCommit);
	tableWidget->addAction(mPush);

	tableWidget->addAction(new QAction);
	tableWidget->actions().back()->setSeparator(true);

	tableWidget->addAction(mOpenRepo);

	tableWidget->addAction(new QAction);
	tableWidget->actions().back()->setSeparator(true);

	tableWidget->addAction(mSetGitExtensions);

	connect(mShowInExplorer, &QAction::triggered,[this](){
		MyQExecute::OpenDir(tableWidget->item(tableWidget->currentRow(),0)->text());
	});

	connect(mUpdateLocal, &QAction::triggered, [this](){ UpdateLocal(tableWidget->currentRow()); });

	connect(mUpdateRemote, &QAction::triggered, [this](){ UpdateRemote(tableWidget->currentRow()); });

	connect(mUpdateLocalAndRemote, &QAction::triggered, [mUpdateLocal, mUpdateRemote](){
		mUpdateLocal->trigger();
		mUpdateRemote->trigger();
	});

	connect(mSetGitExtensions, &QAction::triggered,[this](){
		QString prevGitExtesions;
		prevGitExtesions = GitExtensionsExe;
		GitExtensionsExe.clear();

		ReadAndGetGitExtensionsExe(prevGitExtesions, false);

		if(GitExtensionsExe.isEmpty()) GitExtensionsExe = prevGitExtesions;
	});

	connect(mOpenRepo, &QAction::triggered,[this](){
		QString dir = tableWidget->item(tableWidget->currentRow(),ColIndexes::directory)->text();
		MyQExecute::Execute(ReadAndGetGitExtensionsExe(GitExtensionsExe, true), {dir});
		});

	connect(mCommit, &QAction::triggered,[this](){
		QString dir = tableWidget->item(tableWidget->currentRow(),ColIndexes::directory)->text();
		MyQExecute::Execute(ReadAndGetGitExtensionsExe(GitExtensionsExe, true), {"commit", dir});

//		QString commit_text = MyQDialogs::InputText("Input commit text", "Update", 800, 200);

//		QProcess process;
//		process.setWorkingDirectory(dir);
//		GitStatus addRes = Git::DoGitCommand(process, QStringList() << "add" << ".");
//		GitStatus commitRes;
//		QString textRes = "git add .\n" + addRes.ToStr2();
//		if(addRes.success && addRes.error.isEmpty() && addRes.errorOutput.isEmpty())
//		{
//			commitRes = Git::DoGitCommand(process, QStringList() << "commit" << "-m" << commit_text);
//			textRes += "\n\ngit commit -m "+commit_text+"\n" + commitRes.ToStr2();
//		}
//		mUpdateLocal->trigger();
//		if(!(addRes.success && addRes.error.isEmpty() && addRes.errorOutput.isEmpty()
//				&& commitRes.success && commitRes.error.isEmpty() && commitRes.errorOutput.isEmpty()))
//			textRes.prepend("Attention! There were errors while executing commands!\n\n");
//		MyQDialogs::ShowText(textRes);
	});

	connect(mPush, &QAction::triggered,[this](){
		QString dir = tableWidget->item(tableWidget->currentRow(),ColIndexes::directory)->text();
		MyQExecute::Execute(ReadAndGetGitExtensionsExe(GitExtensionsExe, true), {"push", dir});

//		QString dir = tableWidget->item(tableWidget->currentRow(),ColIndexes::directory)->text();
//		auto remotes = tableWidget->item(tableWidget->currentRow(),ColIndexes::remoteRepos)->text().split(" ", QString::SkipEmptyParts);
//		remotes += "Cancel push";
//		QString remote = MyQDialogs::CustomDialog("Chose remote repo", "Chose remote repo", remotes);
//		if(remote == "Cancel push") return;

//		GitStatus pushRes = Git::DoGitCommand2(dir, QStringList() << "push" << remote << "master");
//		//GitStatus pushRes = Git::DoGitCommand2(dir, QStringList() << "push" << "--recurse-submodules=check" << "--progress" << remote << "master");
//		QString textRes = "git push " + remote + " master\n" + pushRes.ToStr2();
//		if(!(pushRes.success && pushRes.error.isEmpty() && pushRes.errorOutput.isEmpty()))
//			textRes.prepend("Attention! There were errors while executing commands!\n\n");
//		MyQDialogs::ShowText(textRes);
	});
}



void MainWindow::SetRow(int row, const GitStatus & gitStatusResult)
{
	tableWidget->setItem(row, ColIndexes::directory,new QTableWidgetItem(gitStatusResult.dir));
	if(gitStatusResult.error.size())
		tableWidget->setItem(row, ColIndexes::commitStatus,new QTableWidgetItem(gitStatusResult.error));
	else
	{
		tableWidget->setItem(row, ColIndexes::commitStatus,new QTableWidgetItem(gitStatusResult.commitStatus));
		tableWidget->setItem(row, ColIndexes::pushStatus,new QTableWidgetItem(gitStatusResult.pushStatus));
	}
	tableWidget->setItem(row, ColIndexes::remoteRepos		, new QTableWidgetItem(gitStatusResult.RemoteRepoNames()));
	tableWidget->setItem(row, ColIndexes::errorOutput		, new QTableWidgetItem(gitStatusResult.errorOutput));
	tableWidget->setItem(row, ColIndexes::standartOutput	, new QTableWidgetItem(gitStatusResult.standartOutput));
	tableWidget->setItem(row, ColIndexes::remoteOutput		, new QTableWidgetItem);

	if(tableWidget->item(row,ColIndexes::commitStatus)->text() == Statuses::commited
			&& tableWidget->item(row,ColIndexes::pushStatus)->text() == Statuses::pushed)
	{
		tableWidget->item(row,ColIndexes::directory)->setBackground(QColor(146,208,80));
		tableWidget->item(row,ColIndexes::commitStatus)->setBackground(QColor(146,208,80));
		tableWidget->item(row,ColIndexes::pushStatus)->setBackground(QColor(146,208,80));
	}
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
					if(difRes.standartOutput.isEmpty()) { result += "diff empty result\n"; if(curUpdated == fetchEmpty) curUpdated = fetchAndDiffEmpty; }
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

	QColor color(146,208,80);
	for(auto &update:updated) if(update != fetchAndDiffEmpty) { color = {208,146,80}; break; }
	tableWidget->item(row,ColIndexes::remoteRepos)->setBackground(color);
	tableWidget->setCurrentCell(row,0);

	if(stopAll) return stopAllChecks;
	else return ok;
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
		if(QDir(dir).exists())
			allDirs += MyQFileDir::GetAllNestedDirs(dir);
		else badDirs += "\n" + dir;
	}
	if(badDirs.size())
		QMbw(this,"Ошибка","Несуществующие директории:"+badDirs);

	tableWidget->setRowCount(1);
	tableWidget->showRow(0);
	tableWidget->setItem(0,0,new QTableWidgetItem);
	forCommitsParser::mainWindowPtr = this;
	auto progress = [](QString progress)
	{
		forCommitsParser::mainWindowPtr->tableWidget->item(0,0)->setText(progress);
		forCommitsParser::mainWindowPtr->tableWidget->repaint();
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
	//if(tableWidget->rowCount() == 0) return;
	this->setEnabled(false);

	QLabel *labelProgerss = new QLabel("Старт");
	labelProgerss->setWindowFlag(Qt::Tool, true);
	labelProgerss->setAlignment(Qt::AlignCenter);
	auto f = labelProgerss->font(); f.setPointSize(14); labelProgerss->setFont(f);
	labelProgerss->resize(320,100);
	labelProgerss->show();

	MyQTimer *timerChecker = new MyQTimer(this);
	timerChecker->intValues["rowIndex"] = 0;
	int *rowIndexPt = &timerChecker->intValues["rowIndex"];
	connect(timerChecker, &QTimer::timeout, [this, timerChecker, rowIndexPt, labelProgerss](){
		int &rowIndex = *rowIndexPt;

		labelProgerss->activateWindow();
		labelProgerss->setText("Обновляем статус " + QSn(rowIndex) + " из " + QSn(tableWidget->rowCount()));

		while(tableWidget->item(rowIndex,ColIndexes::commitStatus)->text() == GitStatus::notGit)
		{
			rowIndex++;
			if(rowIndex >= tableWidget->rowCount())
			{
				timerChecker->Finish(true);
				labelProgerss->deleteLater();
				this->setEnabled(true);
				return;
			}
		}

		UpdateLocal(rowIndex);
		auto updateRemoteRes = UpdateRemote(rowIndex);
		if(updateRemoteRes == stopAllChecks)
		{
			auto res = MyQDialogs::CustomDialog("Ошибка", "Не удалось обновить удаленный репозиторий",
												{"Перейти к следующему", "Прервать"});
			if(res == "Перейти к следующему") { updateRemoteRes = ok; }
			else if(res == "Прервать") { }
			else QMbc(0,"","wrong answ [" + res + "]");
		}

		rowIndex++;
		if(rowIndex >= tableWidget->rowCount() || updateRemoteRes == stopAllChecks)
		{
			timerChecker->Finish(true);
			labelProgerss->deleteLater();
			this->setEnabled(true);
			return;
		}
	});
	timerChecker->start(25);
}

void MainWindow::SlotHideNotGit()
{
	for(int i=0; i<tableWidget->rowCount(); i++)
		if(tableWidget->item(i,ColIndexes::commitStatus)->text().contains(GitStatus::notGit))
			tableWidget->hideRow(i);
}

void MainWindow::SlotHideCommitedPushed()
{
	for(int i=0; i<tableWidget->rowCount(); i++)
		if(tableWidget->item(i,ColIndexes::commitStatus)->text() == Statuses::commited
				&& tableWidget->item(i,ColIndexes::pushStatus)->text() == Statuses::pushed)
			tableWidget->hideRow(i);
}

void MainWindow::closeEvent(QCloseEvent * event)
{
	ReplaceInTextEdit(textEdit);

	QSettings settings(MyQDifferent::PathToExe() + "/files/settings.ini", QSettings::IniFormat);
	settings.setValue("geo",saveGeometry());
	settings.setValue("splitterCenter",splitterCenter->saveState());
	settings.setValue("textEdit",textEdit->toPlainText());
	settings.setValue("GitExtensionsExe", GitExtensionsExe);
	settings.beginGroup("table");
	for(int i=0; i<tableWidget->columnCount(); i++)
		settings.setValue("col"+QSn(i),tableWidget->columnWidth(i));

	event->accept();
}

void MainWindow::LoadSettings()
{
	QSettings settings(MyQDifferent::PathToExe() + "/files/settings.ini", QSettings::IniFormat);
	this->restoreGeometry(settings.value("geo").toByteArray());
	splitterCenter->restoreState(settings.value("splitterCenter").toByteArray());
	textEdit->setPlainText(settings.value("textEdit").toString());
	GitExtensionsExe = settings.value("GitExtensionsExe").toString();
	settings.beginGroup("table");
	for(int i=0; i<tableWidget->columnCount(); i++)
	{
		if(settings.contains("col"+QSn(i)))
		{
			tableWidget->setColumnWidth(i,settings.value("col"+QSn(i)).toInt());
		}
	}
}
