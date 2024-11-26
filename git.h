#ifndef GIT_H
#define GIT_H

#include <vector>

#include <QDir>
#include <QProcess>
#include <QString>
#include <QtDebug>

#include "MyQShortings.h"

namespace Statuses {
	const QString commited = "commited";
	const QString pushed = "pushed";
}

struct GitStatus
{
	QString dir;

	bool success;

	QString error;

	QString standartOutput;
	QString errorOutput;

	QString commitStatus;
	QString pushStatus;

	QStringList remoteReposes;

	inline static const QString notGit = "not a git repository";
	inline static const QString notGitMarker = "fatal: not a git repository";

	QString ToStr();
};

struct Git
{
	static GitStatus DoGitCommand(QProcess &process, QStringList words);
	static GitStatus DoGitCommand(QString dirFrom, QString command);

	static void DecodeGitCommandResult(GitStatus &gitCommandResult);

	static std::vector<GitStatus> GetGitStatus(const QStringList &dirs, void(*progress)(QString));
};

#endif // GIT_H
