#include "mainwindow.h"

#include <vector>
using namespace std;

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

vector<QString> addTextsForCells;

namespace ColIndexes {
	const int directory = 0;
	const int commitStatus = 1;
	const int pushStatus = 2;
}

namespace Statuses {
	const QString commited = "commited";
	const QString pushed = "pushed";
}

struct GitStatusResult
{
	QString dir;

	bool success;

	QString error;

	QString standartOutput;
	QString errorOutput;

	QString commitStatus;
	QString pushStatus;

	static const QString notGit;
	static const QString notGitMarker;

	QString ToStr()
	{
		return "dir " + dir
				+ "\n success " + QSn(success)
				+ "\n error " + error
				+ "\n outputText " + standartOutput
				+ "\n errorText " + errorOutput;
	}
};

const QString GitStatusResult::notGit = "not a git repository";
const QString GitStatusResult::notGitMarker = "fatal: not a git repository";

GitStatusResult DoGitCommand(QProcess &process, QStringList words)
{
	process.start("git", words);
	if(!process.waitForStarted(1000))
		return { "", false, "error waitForStarted", "", "", "", "" };
	if(!process.waitForFinished(1000))
		return { "", false, "error waitForFinished", "", "", "", "" };
	return { process.workingDirectory(), true, "", process.readAllStandardOutput(), process.readAllStandardError(), "", "" };
}

GitStatusResult DoGitCommand(QString dirFrom, QString command)
{
	QProcess process;
	process.setWorkingDirectory(dirFrom);
	return DoGitCommand(process, command.split(" ", QString::SkipEmptyParts));
}

void DecodeGitCommandResult(GitStatusResult &gitCommandResult)
{
	if(gitCommandResult.errorOutput.isEmpty() && gitCommandResult.standartOutput.isEmpty())
	{
		gitCommandResult.error = "error error.isEmpty() && output.isEmpty()";
	}
	if(gitCommandResult.errorOutput.size() && gitCommandResult.standartOutput.size())
	{
		gitCommandResult.error = "error error.size() && output.size()";
	}

	if(gitCommandResult.errorOutput.size())
	{
		if(gitCommandResult.errorOutput.contains(GitStatusResult::notGitMarker))
			gitCommandResult.commitStatus = GitStatusResult::notGit;
		else
		{
			gitCommandResult.error = "error unknown error ["+gitCommandResult.errorOutput+"]";
		}
	}

	if(gitCommandResult.standartOutput.size())
	{
		if(gitCommandResult.standartOutput.contains("nothing to commit, working tree clean"))
			gitCommandResult.commitStatus = Statuses::commited;
		else if(gitCommandResult.standartOutput.contains("Changes to be committed"))
			gitCommandResult.commitStatus = "not commited, some added";
		else if(gitCommandResult.standartOutput.contains("no changes added to commit"))
			gitCommandResult.commitStatus = "not commited, not added";
		else if(gitCommandResult.standartOutput.contains("nothing added to commit but untracked files present"))
			gitCommandResult.commitStatus = "untracked files";
		else
		{
			gitCommandResult.commitStatus = "error unknown commit output";
		}

		if(gitCommandResult.standartOutput.contains("Your branch is up to date with 'origin/master'"))
			gitCommandResult.pushStatus = Statuses::pushed;
		else if(gitCommandResult.standartOutput.contains("Your branch is ahead of"))
			gitCommandResult.pushStatus = "not pushed";
		else
		{
			auto res = DoGitCommand(gitCommandResult.dir, "remote");
			if(!res.success) { gitCommandResult.pushStatus = "error doing command remote: " + res.error; return; }
			if(!res.errorOutput.isEmpty())  { gitCommandResult.pushStatus = "command remote error output " + res.errorOutput; return; }
			if(res.standartOutput.isEmpty()) gitCommandResult.pushStatus = "no remote repository";
			else gitCommandResult.pushStatus = "error unknown remote output";
		}
	}
}

vector<GitStatusResult> GitStatus(const QStringList &dirs, void(*progress)(QString))
{
	QProcess process;
	vector<GitStatusResult> results;
	results.reserve(dirs.size());
	for(int i=0; i<(int)dirs.size(); i++)
	{
		progress("git status did " + QSn(i) + " from " + QSn(dirs.size()));
		results.push_back(GitStatusResult());
		results.back().dir = dirs[i];

		if(!QDir(dirs[i]+"/.git").exists())
		{
			results.back().commitStatus = GitStatusResult::notGit;
			continue;
		}

		process.setWorkingDirectory(dirs[i]);
		auto gitCmdRes = DoGitCommand(process, QStringList() << "status");
		if(!gitCmdRes.success)
		{
			if(!gitCmdRes.standartOutput.isEmpty()) results.back().error = + " outputText("+gitCmdRes.standartOutput+")";
			if(!gitCmdRes.errorOutput.isEmpty()) results.back().error = + " errorText("+gitCmdRes.errorOutput+")";
			continue;
		}

		DecodeGitCommandResult(gitCmdRes);
		results.erase(results.begin() + results.size()-1);
		results.push_back(gitCmdRes);
		results.back().dir = dirs[i];
	}

	return results;
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
	btn = new QPushButton("Скрыть "+GitStatusResult::notGit, this);
	loRihgtHeader->addWidget(btn);
	connect(btn,&QPushButton::clicked,[this](){
		for(int i=0; i<tableWidget->rowCount(); i++)
			if(tableWidget->item(i,ColIndexes::commitStatus)->text().contains(GitStatusResult::notGit))
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
	tableWidget->setColumnCount(5);
	tableWidget->hideColumn(3);
	tableWidget->hideColumn(4);
	tableWidget->verticalHeader()->hide();
	tableWidget->horizontalHeader()->setStyleSheet("QHeaderView { border-style: none; border-bottom: 1px solid gray; }");
	tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

	connect(tableWidget, &QTableWidget::cellDoubleClicked, [](int row, int column){
		if((int)addTextsForCells.size() > row)
			MyQDialogs::ShowText(addTextsForCells[row],600,300);
		else qdbg << "addTextsForCells.size() <= row";
		if(0) qdbg << column;
	});

	CreateContextMenu();

	LoadSettings();
}

void MainWindow::CreateContextMenu()
{
	QAction *mShowInExplorer = new QAction("Показать в проводнике", tableWidget);
	QAction *mUpdate = new QAction("Обновить статус", tableWidget);
	tableWidget->addAction(mShowInExplorer);
	tableWidget->addAction(mUpdate);
	tableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
	connect(mShowInExplorer, &QAction::triggered,[this](){
		MyQExecute::OpenDir(tableWidget->item(tableWidget->currentRow(),0)->text());
	});

	connect(mUpdate, &QAction::triggered,[this](){
		QString dir = tableWidget->item(tableWidget->currentRow(),ColIndexes::directory)->text();
		QProcess process;
		process.setWorkingDirectory(dir);
		auto res = DoGitCommand(process, QStringList() << "status");
		DecodeGitCommandResult(res);
		res.dir = dir;
		SetRow(tableWidget->currentRow(),res);
	});

	if(0)
	{
		QAction *mAddCommitPush = new QAction("add, commit, push origin master", tableWidget);
		tableWidget->addAction(mAddCommitPush);
		tableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
		connect(mAddCommitPush, &QAction::triggered,[](){
//			QString commit = MyQDialogs::InputText("Введите сообщение коммита:",300,200);
//			if(commit.isEmpty()) { QMbc(this,"Коммит отклонён", "Коммит отклонён, пустое сообщение не допускается"); return; }

//			QProcess process;
//			process.setWorkingDirectory(tableWidget->item(tableWidget->currentRow(),ColIndexes::directory)->text());
//			auto res = DoGitCommand(process, QStringList() << "add" << ".");
//			if(!res.success) { QMbc(this,"Коммит не выполнен", "При выполнении add QProcess ошибки:\n" + res.error); return; }
//			if(!res.error.isEmpty()) { QMbc(this,"Коммит не выполнен", "При выполнении add ошибки:\n" + res.error); return; }
//			if(!res.output.isEmpty()) { QMbc(this,"Коммит не выполнен", "При выполнении add неожиданный вывод:\n" + res.output); return; }

//			res = DoGitCommand(process, QStringList() << "commit" << "-m" << commit);
//			if(!res.success) { QMbc(this,"Коммит не выполнен", "При выполнении commit QProcess ошибки:\n" + res.error); return; }
//			if(!res.error.isEmpty()) { QMbc(this,"Коммит не выполнен", "При выполнении commit ошибки:\n" + res.error); return; }
//			if(res.output.isEmpty()) { QMbc(this,"Коммит не выполнен", "При выполнении commit пустой вывод"); return; }
//			QMbi(this,"Коммит выполнен", "Вывод при выполнении:\n" + res.output);

//			res = DoGitCommand(process, QStringList() << "push" << "origin" << "master");
//			QMbi(this,"Push", res.output + "\n\n\n" + res.error);
//			if(!res.success) { QMbc(this,"Коммит не выполнен", "При выполнении push QProcess ошибки:\n" + res.error); return; }
//			if(!res.error.isEmpty()) { QMbc(this,"Push не выполнен", "При выполнении push ошибки:\n" + res.error); return; }
//			if(res.output.isEmpty()) { QMbc(this,"Push не выполнен", "При выполнении push пустой вывод"); return; }
//			QMbi(this,"Push выполнен", "Push при выполнении:\n" + res.output);
		});
	}
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

void MainWindow::SetRow(int row, const GitStatusResult & gitStatusResult)
{
	tableWidget->setItem(row, ColIndexes::directory,new QTableWidgetItem(gitStatusResult.dir));
	tableWidget->setItem(row, 3, new QTableWidgetItem(gitStatusResult.errorOutput));
	tableWidget->setItem(row, 4, new QTableWidgetItem(gitStatusResult.standartOutput));
	if(gitStatusResult.error.size())
		tableWidget->setItem(row, ColIndexes::commitStatus,new QTableWidgetItem(gitStatusResult.error));
	else
	{
		tableWidget->setItem(row, ColIndexes::commitStatus,new QTableWidgetItem(gitStatusResult.commitStatus));
		tableWidget->setItem(row, ColIndexes::pushStatus,new QTableWidgetItem(gitStatusResult.pushStatus));
	}

	if(tableWidget->item(row,ColIndexes::commitStatus)->text() == Statuses::commited
			&& tableWidget->item(row,ColIndexes::pushStatus)->text() == Statuses::pushed)
	{
		for (int col = 0; col < tableWidget->columnCount(); ++col)
			tableWidget->item(row,col)->setBackground(QColor(146,208,80));
	}

	addTextsForCells[row] = "Output text:\n" + gitStatusResult.standartOutput + "\n\nErrors text:\n" + gitStatusResult.errorOutput;
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
	auto result = GitStatus(allDirs,progress);
	tableWidget->clearContents();
	tableWidget->setRowCount(result.size());

	addTextsForCells.clear();
	addTextsForCells.resize(result.size());

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
