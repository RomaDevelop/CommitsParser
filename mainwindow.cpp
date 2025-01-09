#include "mainwindow.h"

#include <QProcess>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QSettings>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QAction>

#include "MyQShortings.h"
#include "MyQDifferent.h"
#include "MyQFileDir.h"
#include "MyQDialogs.h"
#include "MyQExecute.h"

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
	auto btn = new QPushButton("Сканировать", this);
	loLeft->addWidget(btn);
	connect(btn,&QPushButton::clicked,this,&MainWindow::SlotScan);

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
	connect(btn,&QPushButton::clicked,[this](){
		for(int i=0; i<tableWidget->rowCount(); i++)
			if(tableWidget->item(i,ColIndexes::commitStatus)->text().contains(GitStatus::notGit))
				tableWidget->hideRow(i);
	});
	btn = new QPushButton("Скрыть " + Statuses::commited + " " + Statuses::pushed, this);
	loRihgtHeader->addWidget(btn);
	connect(btn,&QPushButton::clicked,[this](){
		for(int i=0; i<tableWidget->rowCount(); i++)
			if(tableWidget->item(i,ColIndexes::commitStatus)->text() == Statuses::commited
					&& tableWidget->item(i,ColIndexes::pushStatus)->text() == Statuses::pushed)
				tableWidget->hideRow(i);
	});

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
	QAction *mShowInExplorer = new QAction("Показать в проводнике", tableWidget);
	QAction *mUpdate = new QAction("Обновить статус", tableWidget);
	QAction *mUpdateRemote = new QAction("Обновить статус удалённых", tableWidget);
	tableWidget->addAction(mShowInExplorer);
	tableWidget->addAction(mUpdate);
	tableWidget->addAction(mUpdateRemote);
	tableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
	connect(mShowInExplorer, &QAction::triggered,[this](){
		MyQExecute::OpenDir(tableWidget->item(tableWidget->currentRow(),0)->text());
	});

	connect(mUpdate, &QAction::triggered,[this](){
		QString dir = tableWidget->item(tableWidget->currentRow(),ColIndexes::directory)->text();
		QProcess process;
		process.setWorkingDirectory(dir);
		auto gitStatus = Git::GetGitStatusForOneDir(process, dir);
		SetRow(tableWidget->currentRow(),gitStatus);
	});

	connect(mUpdateRemote, &QAction::triggered,[this](){
		tableWidget->item(tableWidget->currentRow(),ColIndexes::remoteRepos)->setBackground(QColor(255,255,255));

		QString dir = tableWidget->item(tableWidget->currentRow(),ColIndexes::directory)->text();
		QStringList remotes = tableWidget->item(tableWidget->currentRow(),ColIndexes::remoteRepos)->text().split(' ',QString::SkipEmptyParts);
		std::vector<int> updated;
		const int undefined = 1;
		const int fetchEmpty = 1;
		const int fetchAndDiffEmpty = 3;
		QProcess process;
		process.setWorkingDirectory(dir);

		QString result;
		for(auto &remote:remotes)
		{
			int &curUpdated = updated.emplace_back(undefined);
			result += "\n\nstart work with " + remote + "\n";
			bool stop = false;
			bool stopAll = false;
			bool did = false;
			while(!stop && !did)
			{
				result += "git fetch " + remote + "\n";
				auto fetchRes = Git::DoGitCommand(process, QStringList() << "fetch" << remote);
				if(fetchRes.success && fetchRes.errorOutput.isEmpty())
				{
					result += fetchRes.standartOutput;
					if(fetchRes.standartOutput.isEmpty()) { result += "fetch empty result\n"; curUpdated = fetchEmpty; }
					did = true;
				}
				else
				{
					if(!fetchRes.error.isEmpty()) result += "fetch error:\n" + fetchRes.error + "\n";
					if(!fetchRes.errorOutput.isEmpty()) result += "fetch errorOutput:\n" + fetchRes.errorOutput + "\n";
					auto res = MyQDialogs::CustomDialog("Ошибка при выполнении fetch",
														"Ошибка при выполнении fetch:\n" + fetchRes.error + "\n" + fetchRes.errorOutput,
														{"Повторить", "Пропустить", "Пропустить всё"});
					if(res == "Повторить") {}
					else if(res == "Пропустить") { stop = true; }
					else if(res == "Пропустить всё") { stop = true; stopAll = true; }
					else QMbc(0,"","wrong answ [" + res + "]");
				}
				if(stop) break;
			}
			if(stopAll) break;

			stop = did = false;
			while(!stop && !did)
			{
				result += "git diff --name-status master "+remote+"/master\n";
				auto difRes = Git::DoGitCommand(process, QStringList() << "diff" << "--name-status" << "master" <<  remote+"/master");
				if(difRes.success && difRes.errorOutput.isEmpty())
				{
					result += difRes.standartOutput;
					if(difRes.standartOutput.isEmpty()) { result += "diff empty result\n"; if(curUpdated == fetchEmpty) curUpdated = fetchAndDiffEmpty; }
					did = true;
				}
				else
				{
					if(!difRes.error.isEmpty()) result += "diff error:\n" + difRes.error + "\n";
					if(!difRes.errorOutput.isEmpty()) result += "diff errorOutput:\n" + difRes.errorOutput + "\n";
					auto res = MyQDialogs::CustomDialog("Ошибка при выполнении diff",
														"Ошибка при выполнении diff:\n" + difRes.error + "\n" + difRes.errorOutput,
														{"Повторить", "Пропустить", "Пропустить всё"});
					if(res == "Повторить") {}
					else if(res == "Пропустить") { stop = true; }
					else if(res == "Пропустить всё") { stop = true; stopAll = true; }
					else QMbc(0,"","wrong answ [" + res + "]");
				}
				if(stop) { stop = false; break; }
			}
			if(stopAll) break;
		}

		tableWidget->item(tableWidget->currentRow(),ColIndexes::remoteOutput)->setText(result);

		QColor color(146,208,80);
		for(auto &update:updated) if(update != fetchAndDiffEmpty) { color = {208,146,80}; break; }
		tableWidget->item(tableWidget->currentRow(),ColIndexes::remoteRepos)->setBackground(color);
		tableWidget->setCurrentCell(tableWidget->currentRow(),0);
	});



	// add, commit, push origin master
//	QAction *mAddCommitPush = new QAction("add, commit, push origin master", tableWidget);
//	tableWidget->addAction(mAddCommitPush);
//	tableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
//	connect(mAddCommitPush, &QAction::triggered,[](){
//		QString commit = MyQDialogs::InputText("Введите сообщение коммита:",300,200);
//		if(commit.isEmpty()) { QMbc(this,"Коммит отклонён", "Коммит отклонён, пустое сообщение не допускается"); return; }

//		QProcess process;
//		process.setWorkingDirectory(tableWidget->item(tableWidget->currentRow(),ColIndexes::directory)->text());
//		auto res = DoGitCommand(process, QStringList() << "add" << ".");
//		if(!res.success) { QMbc(this,"Коммит не выполнен", "При выполнении add QProcess ошибки:\n" + res.error); return; }
//		if(!res.error.isEmpty()) { QMbc(this,"Коммит не выполнен", "При выполнении add ошибки:\n" + res.error); return; }
//		if(!res.output.isEmpty()) { QMbc(this,"Коммит не выполнен", "При выполнении add неожиданный вывод:\n" + res.output); return; }

//		res = DoGitCommand(process, QStringList() << "commit" << "-m" << commit);
//		if(!res.success) { QMbc(this,"Коммит не выполнен", "При выполнении commit QProcess ошибки:\n" + res.error); return; }
//		if(!res.error.isEmpty()) { QMbc(this,"Коммит не выполнен", "При выполнении commit ошибки:\n" + res.error); return; }
//		if(res.output.isEmpty()) { QMbc(this,"Коммит не выполнен", "При выполнении commit пустой вывод"); return; }
//		QMbi(this,"Коммит выполнен", "Вывод при выполнении:\n" + res.output);

//		res = DoGitCommand(process, QStringList() << "push" << "origin" << "master");
//		QMbi(this,"Push", res.output + "\n\n\n" + res.error);
//		if(!res.success) { QMbc(this,"Коммит не выполнен", "При выполнении push QProcess ошибки:\n" + res.error); return; }
//		if(!res.error.isEmpty()) { QMbc(this,"Push не выполнен", "При выполнении push ошибки:\n" + res.error); return; }
//		if(res.output.isEmpty()) { QMbc(this,"Push не выполнен", "При выполнении push пустой вывод"); return; }
//		QMbi(this,"Push выполнен", "Push при выполнении:\n" + res.output);
//	});
}

void MainWindow::LoadSettings()
{
	QSettings settings(MyQDifferent::PathToExe() + "/files/settings.ini", QSettings::IniFormat);
	this->restoreGeometry(settings.value("geo").toByteArray());
	splitterCenter->restoreState(settings.value("splitterCenter").toByteArray());
	textEdit->setPlainText(settings.value("textEdit").toString());
	settings.beginGroup("table");
	for(int i=0; i<tableWidget->columnCount(); i++)
	{
		if(settings.contains("col"+QSn(i)))
		{
			tableWidget->setColumnWidth(i,settings.value("col"+QSn(i)).toInt());
		}
	}
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

void MainWindow::closeEvent(QCloseEvent * event)
{
	ReplaceInTextEdit(textEdit);

	QSettings settings(MyQDifferent::PathToExe() + "/files/settings.ini", QSettings::IniFormat);
	settings.setValue("geo",saveGeometry());
	settings.setValue("splitterCenter",splitterCenter->saveState());
	settings.setValue("textEdit",textEdit->toPlainText());
	settings.beginGroup("table");
	for(int i=0; i<tableWidget->columnCount(); i++)
		settings.setValue("col"+QSn(i),tableWidget->columnWidth(i));

	event->accept();
}
