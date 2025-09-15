#include "git.h"


QString GitStatus::RemoteRepoNames() const
{
	QString ret;
	for(auto &remoteRepo:remoteRepos)
	{
		ret += remoteRepo.name + " ";
	}
	while(ret.endsWith(" ")) ret.chop(1);
	return ret;
}

QString GitStatus::ToStr()
{
	return "dir: " + dir
			+ "\nsuccess: " + QSn(success)
			+ "\nerror: " + (error.isEmpty() ? "empty" : "\n"+error+"\n")
			+ "\noutputText: " + (standartOutput.isEmpty() ? "empty" : "\n"+standartOutput+"\n")
			+ "\nerrorText: " + (errorOutput.isEmpty() ? "empty" : "\n"+errorOutput+"\n");
}

QString GitStatus::ToStr2()
{
	QString str = "dir: " + dir + (success ? "\nsuccess" : "\nunsuccess");
	if(!error.isEmpty()) str += "\nerror: "+error;
	if(!standartOutput.isEmpty()) str += "\nstandartOutput: "+standartOutput;
	if(!errorOutput.isEmpty()) str += "\nerrorOutput: "+errorOutput;
	return str;
}

GitStatus Git::DoGitCommand(QProcess & process, QStringList words)
{
	process.start("git", words);
	if(!process.waitForStarted(5000))
	{
		qdbg << "error waitForStarted " + words.join(" ");
		return { "", false, "error waitForStarted", "", "", "", "", {}};
	}
	if(!process.waitForFinished(10000))
	{
		qdbg << "error waitForFinished " + words.join(" ");
		return { "", false, "error waitForFinished", "", "", "", "", {}};
	}
	return { process.workingDirectory(), true, "", process.readAllStandardOutput(), process.readAllStandardError(), "", "", {}};
}

GitStatus Git::DoGitCommand2(QString dirFrom, QStringList words)
{
	QProcess process;
	process.setWorkingDirectory(dirFrom);
	return DoGitCommand(process, words);
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
			gitCommandResult.commitStatus = Statuses::commited();
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

		gitCommandResult.pushStatus = DefineMainRemoteStatus(gitCommandResult.standartOutput);
		if(gitCommandResult.pushStatus.isEmpty())
		{
			auto res = DoGitCommand2(gitCommandResult.dir, {"remote"});
			if(!res.success) { gitCommandResult.pushStatus = "error doing command remote: " + res.error; return; }
			if(!res.errorOutput.isEmpty())  { gitCommandResult.pushStatus = "command remote error output " + res.errorOutput; return; }
			if(res.standartOutput.isEmpty()) gitCommandResult.pushStatus = "no remote repository";
			else gitCommandResult.pushStatus = "error unknown remote output";
		}
	}
}

std::vector<GitStatus> Git::GetGitStatus(const QStringList & dirs, std::function<void(int did)> progress)
{
	QProcess process;
	std::vector<GitStatus> results;
	results.reserve(dirs.size());
	for(int i=0; i<(int)dirs.size(); i++)
	{
		results.emplace_back(GetGitStatusForOneDir(process, dirs[i]));

//		for(auto &remoteRepo:results.back().remoteReposes)
//		{
//			gitCmdRes = DoGitCommand(process, QStringList() << "diff" << "--name-status" << "master" << remoteRepo+"/master");
//			if(!(gitCmdRes.standartOutput.isEmpty() && gitCmdRes.errorOutput.isEmpty()))
//			{
//				qdbg << "___________________________";
//				qdbg << dirs[i];
//				qdbg << remoteRepo;
//				qdbg << "success		" << gitCmdRes.success;
//				qdbg << "standartOutput	" << gitCmdRes.standartOutput;
//				qdbg << "errorOutput	" << gitCmdRes.errorOutput;
//			}
//		}
		progress(i);
	}

	return results;
}

GitStatus Git::GetGitStatusForOneDir(QProcess &process, const QString &dir)
{
	GitStatus gitStatus;
	gitStatus.dir = dir;

	if(!QFileInfo(QString(dir).append('/').append(GitStatus::gitRepoDir())).isDir())
	{
		gitStatus.commitStatus = GitStatus::notGit;
		return gitStatus;
	}

	process.setWorkingDirectory(dir);
	auto gitCmdRes = DoGitCommand(process, QStringList() << "status");
	if(!gitCmdRes.success)
	{
		if(!gitCmdRes.standartOutput.isEmpty()) gitStatus.error = + " outputText("+gitCmdRes.standartOutput+")";
		if(!gitCmdRes.errorOutput.isEmpty()) gitStatus.error = + " errorText("+gitCmdRes.errorOutput+")";
		return gitStatus;
	}

	DecodeGitCommandResult(gitCmdRes);
	gitStatus = gitCmdRes;

	gitCmdRes = DoGitCommand(process, QStringList() << "remote");
	if(gitCmdRes.success && gitCmdRes.errorOutput.isEmpty())
	{
		auto names = gitCmdRes.standartOutput.split("\n", QString::SkipEmptyParts);
		for(auto &name:names)
		{
			RemoteRepo &newRepo = gitStatus.remoteRepos.emplace_back();
			newRepo.name = std::move(name);
		}
	}
	else
	{
		RemoteRepo &newRepo = gitStatus.remoteRepos.emplace_back();
		newRepo.errors = "remote error\n";
	}

	return gitStatus;
}

GitStatus Git::GetGitStatusForOneDir(const QString &dir)
{
	QProcess process;
	return GetGitStatusForOneDir(process, dir);
}



