#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QCloseEvent>
#include <QSplitter>
#include <QTextEdit>
#include <QTableWidget>

struct GitStatus;

class MainWindow : public QWidget
{
	Q_OBJECT

	QSplitter *splitterCenter;
	QTextEdit *textEdit;
	QTableWidget *tableWidget;
	QString GitExtensionsExe;
	QString ReadAndGetGitExtensionsExe(QString dir, bool showInfoMessageBox);
public:
	MainWindow(QWidget *parent = nullptr);
	void CreateContextMenu();
	~MainWindow() = default;
	void closeEvent(QCloseEvent * event);
	void LoadSettings();

	void SetRow(int row, const GitStatus &gitStatusResult);

	void UpdateLocal(int row);
	enum UpdateRemoteRes { ok, stopAllChecks };
	UpdateRemoteRes UpdateRemote(int row);

private: signals:

private slots:
	void SlotScan();
	void SlotCheckRemotes();
	void SlotHideNotGit();
	void SlotHideCommitedPushed();
};
#endif // MAINWINDOW_H
