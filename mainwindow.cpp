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

#include "MyQShortings.h"
#include "MyQDifferend.h"
#include "MyQFileDir.h"
#include "MyQDialogs.h"

struct GitStatusResult
{
	QString dir;

	QString error;

	QString errorText;
	QString outputText;

	QString commitStatus;
	QString pushStatus;

	inline static const QString notGit = "not a git repository";
	inline static const QString notGitMarker = "fatal: not a git repository";
};

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
		process.start("git", QStringList() << "status");
		if(!process.waitForStarted(1000))
		{
			results.back().error = "error waitForStarted";
			continue;
		}
		if(!process.waitForFinished(1000))
		{
			results.back().error = "error waitForFinished";
			continue;
		}

		results.back().errorText = process.readAllStandardError();
		results.back().outputText = process.readAllStandardOutput();


		if(results.back().errorText.isEmpty() && results.back().outputText.isEmpty())
		{
			results.back().error = "error error.isEmpty() && output.isEmpty()";
			continue;
		}
		if(results.back().errorText.size() && results.back().outputText.size())
		{
			results.back().error = "error error.size() && output.size()";
			continue;
		}


		if(results.back().errorText.size())
		{
			if(results.back().errorText.contains(GitStatusResult::notGitMarker))
				results.back().commitStatus = GitStatusResult::notGit;
			else
			{
				results.back().error = "error unknown error";
				continue;
			}
		}


		if(results.back().outputText.size())
		{
			if(results.back().outputText.contains("nothing to commit, working tree clean"))
				results.back().commitStatus = "commited";
			else if(results.back().outputText.contains("no changes added to commit"))
				results.back().commitStatus = "not commited, not added";
			else
			{
				results.back().commitStatus = "error unknown commit output";
			}

			if(results.back().outputText.contains("Your branch is up to date with"))
				results.back().pushStatus = "pushed";
			else if(results.back().outputText.contains("Your branch is ahead of"))
				results.back().pushStatus = "not pushed";
			else
			{
				results.back().pushStatus = "error unknown push output";
			}
		}
	}
	return results;
}

namespace forProgress {
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

	auto widgetLeft = new QWidget(this);
	auto widgetRight = new QWidget(this);
	splitterCenter->addWidget(widgetLeft);
	splitterCenter->addWidget(widgetRight);
	auto loLeft = new QVBoxLayout(widgetLeft);
	auto loRight = new QVBoxLayout(widgetRight);

	// left part
	int commitStatusCol = 1;

	loLeft->addWidget(new QLabel("Сканируемые пути",this));
	textEdit = new QTextEdit(this);
	loLeft->addWidget(textEdit);
	textEdit->setFont(basicFont);
	auto btn = new QPushButton("Сканировать", this);
	loLeft->addWidget(btn);
	connect(btn,&QPushButton::clicked,[this, commitStatusCol](){
		tableWidget->clearContents();
		auto dirs = textEdit->toPlainText().split('\n');
		QStringList allDirs;
		QString badDirs;
		for(auto &dir:dirs)
		{
			if(QDir(dir).exists())
				allDirs += MQFileDir::GetAllNestedDirs(dir);
			else badDirs += "\n" + dir;
		}
		if(badDirs.size())
			QMbw(this,"Ошибка","Несуществующие директории:"+badDirs);

		tableWidget->setRowCount(1);
		tableWidget->showRow(0);
		tableWidget->setItem(0,0,new QTableWidgetItem);
		forProgress::mainWindowPtr = this;
		auto progress = [](QString progress)
		{
			forProgress::mainWindowPtr->tableWidget->item(0,0)->setText(progress);
			forProgress::mainWindowPtr->tableWidget->repaint();
		};
		auto result = GitStatus(allDirs,progress);
		tableWidget->clearContents();
		tableWidget->setRowCount(result.size());
		for(int i=0; i<(int)result.size(); i++)
		{
			tableWidget->setItem(i,0,new QTableWidgetItem(result[i].dir));
			tableWidget->setItem(i,3,new QTableWidgetItem(result[i].errorText));
			tableWidget->setItem(i,4,new QTableWidgetItem(result[i].outputText));
			if(result[i].error.size())
				tableWidget->setItem(i,1,new QTableWidgetItem(result[i].error));
			else
			{
				tableWidget->setItem(i,commitStatusCol,new QTableWidgetItem(result[i].commitStatus));
				tableWidget->setItem(i,2,new QTableWidgetItem(result[i].pushStatus));
			}
		}
	});

	// right part
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
	connect(btn,&QPushButton::clicked,[this,commitStatusCol](){
		for(int i=0; i<tableWidget->rowCount(); i++)
			if(tableWidget->item(i,commitStatusCol)->text().contains(GitStatusResult::notGit))
				tableWidget->hideRow(i);
	});
	tableWidget = new QTableWidget(this);
	loRight->addWidget(tableWidget);
	tableWidget->setFont(basicFont);
	tableWidget->setColumnCount(5);
	tableWidget->hideColumn(3);
	tableWidget->hideColumn(4);
	tableWidget->verticalHeader()->hide();
	tableWidget->horizontalHeader()->setStyleSheet("QHeaderView { border-style: none; border-bottom: 1px solid gray; }");

	LoadSettings();
}

void MainWindow::LoadSettings()
{
	QSettings settings(MyQDifferent::GetPathToExe() + "/files/settings.ini", QSettings::IniFormat);
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

void MainWindow::closeEvent(QCloseEvent * event)
{
	QSettings settings(MyQDifferent::GetPathToExe() + "/files/settings.ini", QSettings::IniFormat);
	settings.setValue("geo",saveGeometry());
	settings.setValue("splitterCenter",splitterCenter->saveState());
	settings.setValue("textEdit",textEdit->toPlainText());
	settings.beginGroup("table");
	for(int i=0; i<tableWidget->columnCount(); i++)
		settings.setValue("col"+QSn(i),tableWidget->columnWidth(i));

	event->accept();
}
