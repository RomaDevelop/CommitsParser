#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QCloseEvent>
#include <QSplitter>
#include <QTextEdit>
#include <QTableWidget>

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

private: signals:

private slots:
	void SlotScan();
};
#endif // MAINWINDOW_H
