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

struct RemoteRepo
{
	QString name;
	QString errors;
	QString fetchRes;
	QString diffRes;
	bool updated = false;
};

struct GitStatus
{
	QString dir;

	bool success;

	QString error;

	QString standartOutput;
	QString errorOutput;

	QString commitStatus;
	QString pushStatus;

	std::vector<RemoteRepo> remoteRepos;
	QString RemoteRepoNames() const;

	inline static const QString notGit = "not a git repository";
	inline static const QString notGitMarker = "fatal: not a git repository";

	QString ToStr();
	QString ToStr2();
};

struct Git
{
	static GitStatus DoGitCommand(QProcess &process, QStringList words);
	static GitStatus DoGitCommand2(QString dirFrom, QStringList words);

	static void DecodeGitCommandResult(GitStatus &gitCommandResult);

	static std::vector<GitStatus> GetGitStatus(const QStringList &dirs, void(*progress)(QString));
	static GitStatus GetGitStatusForOneDir(QProcess &process, const QString &dir);
};

#endif // GIT_H
