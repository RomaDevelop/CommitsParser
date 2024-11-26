#include "git.h"


QString GitStatus::ToStr()
{
	return "dir " + dir
			+ "\n success " + QSn(success)
			+ "\n error " + error
			+ "\n outputText " + standartOutput
			+ "\n errorText " + errorOutput;
}

GitStatus Git::DoGitCommand(QProcess & process, QStringList words)
{
	process.start("git", words);
	if(!process.waitForStarted(1000))
		return { "", false, "error waitForStarted", "", "", "", "", {} };
	if(!process.waitForFinished(1000))
		return { "", false, "error waitForFinished", "", "", "", "", {} };
	return { process.workingDirectory(), true, "", process.readAllStandardOutput(), process.readAllStandardError(), "", "", {} };
}

GitStatus Git::DoGitCommand(QString dirFrom, QString command)
{
	QProcess process;
	process.setWorkingDirectory(dirFrom);
	return DoGitCommand(process, command.split(" ", QString::SkipEmptyParts));
}

void Git::DecodeGitCommandResult(GitStatus & gitCommandResult)
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
		if(gitCommandResult.errorOutput.contains(GitStatus::notGitMarker))
			gitCommandResult.commitStatus = GitStatus::notGit;
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

std::vector<GitStatus> Git::GetGitStatus(const QStringList & dirs, void (*progress)(QString))
{
	QProcess process;
	std::vector<GitStatus> results;
	results.reserve(dirs.size());
	for(int i=0; i<(int)dirs.size(); i++)
	{
		progress("git status did " + QSn(i) + " from " + QSn(dirs.size()));
		results.push_back(GitStatus());
		results.back().dir = dirs[i];

		if(!QDir(dirs[i]+"/.git").exists())
		{
			results.back().commitStatus = GitStatus::notGit;
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

		gitCmdRes = DoGitCommand(process, QStringList() << "remote");
		if(gitCmdRes.success && gitCmdRes.errorOutput.isEmpty())
			results.back().remoteReposes = gitCmdRes.standartOutput.split("\n", QString::SkipEmptyParts);
	}

	return results;
}


