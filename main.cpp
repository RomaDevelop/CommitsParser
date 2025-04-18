#include "mainwindow.h"

#include <QDebug>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>

#include "MyQShortings.h"
#include "MyQDialogs.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	if(argc == 1)
	{
		MainWindow *w = new MainWindow;
		w->show();
	}
	if(argc >= 2)
	{
		if(QString(argv[1]) == "scan_and_check_remotes_on_start")
		{
			MainWindow::LaunchAndStartScan();
		}
		else if(QString(argv[1]) == "question_before_launch")
		{
			QString answ = MyQDialogs::CustomDialog("Commits parser launch", "Choose CommitsParser launch option",
													{"Launch", "Launch and start scan", "Abort launch"});
			if(answ == "Launch")
			{
				MainWindow *w = new MainWindow;
				w->show();
			}
			else if(answ == "Launch and start scan")
			{
				MainWindow::LaunchAndStartScan();
			}
			else if("Abort launch")
			{
				QTimer::singleShot(0,[](){ QApplication::exit(); });
			}
			else QMbError("unrealesed action " + answ);
		}
		else QMbError("wrong arg: " + QString(argv[1]));
	}

	if(argc > 2) QMbError("wrong argc: " + QSn(argc));

	return a.exec();
}
