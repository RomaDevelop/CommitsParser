#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QCloseEvent>
#include <QSplitter>
#include <QTextEdit>
#include <QTableWidget>

#include "MyQDifferent.h"

struct GitStatus;

class MainWindow : public QWidget
{
	Q_OBJECT

	QSplitter *splitterCenter;
	QTextEdit *textEdit;
	QTableWidget *tableWidget;
	QCheckBox *chBoxStopAtCount = nullptr;
	QLineEdit *leCountToStopAt = nullptr;
	QString GitExtensionsExe;
	QString filesPath = MyQDifferent::PathToExe() + "/files";
	QString ReadAndGetGitExtensionsExe(QString dir, bool showInfoMessageBox);
public:
	MainWindow(QWidget *parent = nullptr);
	static void LaunchAndStartScan();
	void CreateContextMenu();
	~MainWindow() = default;
	void closeEvent(QCloseEvent * event);
	void LoadSettings();

	void SetRow(int row, const GitStatus &gitStatusResult);

	void UpdateLocal(int row);
	enum UpdateRemoteRes { ok, stopAllChecks };
	UpdateRemoteRes UpdateRemote(int row);

public slots:
	void SlotScan();
	void SlotCheckRemotes();
	void SlotScanAndCheckRemotes();
	void SlotScanAndCheckRemotesCurrent();

	void SlotHideNotGit();
	void SlotHideCommitedPushedRemoteOk();
};
#endif // MAINWINDOW_H
