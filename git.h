#ifndef GIT_H
#define GIT_H

#include <vector>

#include <QDir>
#include <QProcess>
#include <QString>
#include <QtDebug>

#include "MyQShortings.h"

namespace Statuses {
	inline const QString& commited()		{ static QString str = "commited"; return str; }
	inline const QString& pushed()			{ static QString str = "pushed"; return str; }
	inline const QString& not_pushed()		{ static QString str = "not pushed"; return str; }
	inline const QString& behind()			{ static QString str = "behind"; return str; }
	inline const QString& behind_unknown()	{ static QString str = "behind+unknown"; return str; }
	inline const QString& empty_str()		{ static QString str = ""; return str; }
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
	static const QString& DefineMainRemoteStatus(const QString &standartOutput)
	{
		if(standartOutput.contains("Your branch is up to date with '"))
			return Statuses::pushed();

		if(standartOutput.contains("Your branch is ahead of"))
			return Statuses::not_pushed();

		if(standartOutput.contains("Your branch is behind"))
		{
			if(standartOutput.contains("and can be fast-forwarded."))
				return Statuses::behind();
			else return Statuses::behind_unknown();
		}

		return Statuses::empty_str();
	}

	static std::vector<GitStatus> GetGitStatus(const QStringList &dirs, void(*progress)(QString));
	static GitStatus GetGitStatusForOneDir(QProcess &process, const QString &dir);
};

#endif // GIT_H
