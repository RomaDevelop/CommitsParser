#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QCloseEvent>
#include <QSplitter>
#include <QTextEdit>
#include <QTableWidget>

struct GitStatusResult;

class MainWindow : public QWidget
{
	Q_OBJECT

	QSplitter *splitterCenter;
	QTextEdit *textEdit;
	QTableWidget *tableWidget;
public:
	MainWindow(QWidget *parent = nullptr);
	void CreateContextMenu();
	~MainWindow() = default;
	void closeEvent(QCloseEvent * event);
	void LoadSettings();

	void SetRow(int row, const GitStatusResult &gitStatusResult);

private: signals:

private slots:
	void SlotScan();
};
#endif // MAINWINDOW_H
