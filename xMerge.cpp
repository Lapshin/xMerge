#include <iostream>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <string>
#include <regex>
#include <list>

using namespace std;

void sorryButExit(int value)
{
	if(value != 0) {
		cerr << endl <<"More info on http://red.eltex.loc/projects/gpon/wiki/XMerge" << endl;
	}
	exit(value);
}

void ReplaceStringInPlace(std::string& subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

class SvnRevisionInfo
{
public:
	unsigned revision;
	string author;
	string branch;
	string message;
	SvnRevisionInfo(unsigned r){ revision = r; branch = "Not found"; };
};

class SvnInfo
{
private:
	string repos;
	string propsPath;
	string txn;
	unsigned shard;
	string message;
	string buildedMessage;
	string actionStr;
	vector<unsigned> revisions;
public:
	SvnInfo(string, string);
	void extractValueOfKey(string path, string key, string &value);
	void buildMessage();
	void editTransactionInfo();
	void getSharded();
	bool isItMyProject();
	void checkSvnMessage();
	void parseXMergeRevisions();
};

void SvnInfo::parseXMergeRevisions()
{
	stringstream regexStram;
	string s = this->message;
	transform(s.begin(), s.end(), s.begin(), ::toupper);

	s = regex_replace(s, regex("\\s*,*"), "");

	regexStram << "^\\[X" << this->actionStr<< ".*?\\]";
	regex xmergeRegx = regex(regexStram.str());
	smatch m;

	if (regex_search(s, m, xmergeRegx) == true)
	{
		size_t start = 0, end = 0;
		string tmp;
		string x = string(m[0]);
		x.erase(x.length() - 1, x.length());
		x.erase(0, this->actionStr.length() + 2);

		if (regex_match(x, regex("(R[[:digit:]]+[-]R[[:digit:]]+|R[[:digit:]]+)+")) == false)
		{
			cerr << "Revision syntax check failed (" << x << ")" << endl;
			sorryButExit(1);
		}

		unsigned range_start, range_end;
		while (1)
		{
			range_start = 0;
			range_end = 0;
			if(start == 0)
			{
				start = x.find_first_of('R', 0);
				if(start == string::npos)
				{
					cerr << "No revisions found in revisions enumeration" << endl;
					sorryButExit(1);
				}
				start += 1;
				continue;
			}
			end = x.find_first_of('R', start);

			tmp = x.substr(start, end - start);
			if (tmp.find_last_of('-', end) != string::npos)
			{
				end = x.find_first_of('R', end + 1);
				tmp = x.substr(start, end - start);
				tmp = regex_replace(tmp, regex("-R"), " ");
			}

			stringstream(tmp) >> range_start >> range_end;
			this->revisions.push_back(range_start);
			if (range_end != 0)
			{
				if (range_start > range_end)
				{
					cerr << "Wrong range used: start revision greater then end of revisions range" << endl;
					sorryButExit(1);
				}
				for (unsigned i = range_start + 1; i <= range_end; i++)
				{
					this->revisions.push_back(i);
				}
			}
			if(end == string::npos)
			{
				break;
			}
			start = end + 1;

		}
		sort(this->revisions.begin(), this->revisions.end());
		this->revisions.erase(unique(this->revisions.begin(), this->revisions.end()), this->revisions.end());
	}
}

void SvnInfo::extractValueOfKey(string path, string key, string &value) {
	int valueLength;
	string tmp;
	ifstream fin(path);
	if (fin.is_open() == false) {
		cerr << "Can't open " << path << endl;
		sorryButExit(1);
	}
	string keyStr = "K ";
	stringstream keyStream;
	keyStream << keyStr << key.length();
	keyStr = keyStream.str();
	while(getline(fin, tmp)) {
		if (tmp.compare(keyStr) == 0 && getline(fin, tmp)
				&& tmp.compare(key) == 0 && getline(fin, tmp)) {
			if (tmp.compare(0, strlen("V "), "V ") != 0) {
				cerr << "Unexpected string after \"K\" " << tmp << endl;
				sorryButExit(1);
			}
			tmp.erase(0, strlen("V "));
			stringstream(tmp) >> valueLength;
			char* buf = (char*) (calloc(valueLength, 1));
			fin.read(buf, valueLength);
			value = buf;
			free(buf);
			break;
		}
	}
}

void SvnInfo::buildMessage() {

	if (this->revisions.size() == 0) {
		cerr << "Unexpected value of revisions count" << endl;
		sorryButExit(1);
	} else if(this->revisions.size() > 50) {
		cerr << "Too many revisions in your message. Limit is 50" << endl;
		sorryButExit(1);
	}

	stringstream mergeStream;
	mergeStream << "[" << this->actionStr;
	for (unsigned i = 0; i < this->revisions.size(); i++) {
		if (i > 0) {
			mergeStream << ",";
		}
		mergeStream << " r" << this->revisions.at(i);
	}
	mergeStream << this->message.substr(this->message.find_first_of(']'));
	for (unsigned i = 0; i < this->revisions.size(); i++) {
		SvnRevisionInfo rInfo = SvnRevisionInfo(this->revisions.at(i));
		unsigned revShardFolder = this->shard == 0 ? 0 : (rInfo.revision / this->shard);
		string keyAuthor = "svn:author";
		string keyLog = "svn:log";
		stringstream revpropsPath;
		revpropsPath << this->repos << "/db/revprops/" << revShardFolder << "/" << rInfo.revision;
		string revpropsPath_s = revpropsPath.str();
		extractValueOfKey(revpropsPath_s, keyAuthor, rInfo.author);
		extractValueOfKey(revpropsPath_s, keyLog, rInfo.message);
		stringstream revsPath;
		revsPath << this->repos << "/db/revs/" << revShardFolder << "/" << rInfo.revision;
		string revsPath_s = revsPath.str();
		ifstream fin(revsPath_s);
		if (fin.is_open() == false) {
			cerr << "Can't open " << revsPath_s << endl;
			sorryButExit(1);
		}
		string tmp;
		string cpath = "cpath:";
		regex reg_ex(".*/branches/.*?/|.*trunk/");
		smatch m;
		while (getline(fin, tmp)) {
			if (tmp.compare(0, cpath.length(), cpath) == 0) {
				string empty_s;
				stringstream(tmp) >> empty_s >> tmp;

				if (regex_search(tmp, m, reg_ex) == true) {
					rInfo.branch = string(m[0]);
					break;
				}
			}
		}
		fin.close();

		rInfo.message.insert(0, "\t");
		ReplaceStringInPlace(rInfo.message, string("\n"), string( "\n\t"));

		mergeStream << endl << "r" << rInfo.revision << "::" << rInfo.branch
				<< "::" << rInfo.author << endl << rInfo.message;
	}
	this->buildedMessage = mergeStream.str();
}

void SvnInfo::editTransactionInfo() {
	string tmpFile = this->propsPath;
	string key = "svn:log";
	string keyStr = "K ";
	stringstream keyStream;
	keyStream << keyStr << key.length();
	keyStr = keyStream.str();
	tmpFile.append(".tmp");
	ifstream fin(this->propsPath);
	ofstream of(tmpFile);
	string tmp;
	while (getline(fin, tmp)) {
		if (tmp.compare(keyStr) == 0 && getline(fin, tmp)
				&& tmp.compare(key) == 0 && getline(fin, tmp)) {
			if (tmp.compare(0, strlen("V "), "V ") != 0) {
				cerr << "Unexpected string after \"K\" " << tmp << endl;
				sorryButExit(1);
			}
			of << keyStr << endl;
			of << key << endl;
			of << "V " << this->buildedMessage.length() << endl;
			of.write(this->buildedMessage.c_str(),
					this->buildedMessage.length());
			fin.seekg(this->message.length() , fin.cur);
			continue;
		}
		of << tmp << endl;
	}
	fin.close();
	of.close();
	remove(this->propsPath.c_str());
	rename(tmpFile.c_str(), this->propsPath.c_str());
}

void SvnInfo::getSharded() {
	string tmp;
	string formatPath = this->repos;
	formatPath.append("/db/format");
	ifstream fin(formatPath);
	if (fin.is_open() == false) {
		cerr << "Can't open " << formatPath << endl;
		sorryButExit(1);
	}
	this->shard = 0;
	string s_token = string("layoutsharded");
	while (getline(fin, tmp)) {
		tmp = regex_replace(tmp, regex("[[:space:]]*"), "");
		transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
		if (tmp.compare(0, s_token.length(), s_token) == 0) {
			tmp.erase(0, s_token.length());
			stringstream(tmp) >> this->shard;
			break;
		}
	}
	fin.close();
}

bool SvnInfo::isItMyProject() {
	string changesPath = this->repos;
	changesPath.append("/db/transactions/" + this->txn + ".txn/changes");
	bool isMyProject = false;
	string tmp;
	ifstream fin(changesPath);
	if (fin.is_open() == false) {
		cerr << "Can't open " << changesPath << " (bad transaction)" << endl;
		sorryButExit(1);
	}
	regex myProjects(".*gpon.*|.*ma4000.*");
	while (getline(fin, tmp)) {
		isMyProject = regex_match(tmp, myProjects);
		if (isMyProject == true) {
			break;
		}
	}
	fin.close();
	return isMyProject;
}

void SvnInfo::checkSvnMessage() {
	bool xMerge;
	bool xRevert;
	bool printUsage = false;
	string msg = this->message;
	transform(msg.begin(), msg.end(), msg.begin(), ::tolower);
	ReplaceStringInPlace(msg, "\n", "");
	regex xRefs(".*(refs|fixes)[[:space:]]#[[:digit:]]+([[:space:]]|\\)|$).*");
	if (regex_match((msg), xRefs) == false) {
		cerr << "You MUST set reference to redmine issue." << endl <<
				"Use keywords \"refs\" or \"fixes\"" << endl <<
				"Example: refs #69" << endl;
		sorryButExit(1);
	}
	regex xMerge_reg(".*\\[.*xmerge[[:space:]].*?\\].*");
	xMerge = regex_match((msg), xMerge_reg);

	regex xRevert_reg(".*\\[.*xrevert[[:space:]].*?\\].*");
	xRevert = regex_match((msg), xRevert_reg);

	regex merge_reg(".*merge.*");
	if (xMerge == false && regex_match((msg), merge_reg) == true) {
		cerr << "Use xMERGE instead merge" << endl;
		cerr << "xMERGE example: [xmerge ";
		printUsage = true;
	}
	regex revert_reg(".*revert.*");
	if (printUsage == false && xRevert == false && regex_match((msg), revert_reg) == true) {
		cerr << "Use xREVERT instead revert" << endl;
		cerr << "xREVERT example: [xrevert ";
		printUsage = true;
	}

	if(printUsage == true) {
		cerr << " r1, r2, r20-r21, r5] Your original comment bla-bla refs #0000" << endl;
		sorryButExit(1);
	}

	if (xMerge == true && xRevert == true) {
		cerr << "Don't merge and revert at the same time" << endl;
		sorryButExit(1);
	} else if(xMerge == false && xRevert == false) {
		sorryButExit(0);
	} else if(xMerge == true) {
		this->actionStr = "MERGE";
	} else {
		this->actionStr = "REVERT";
	}

	return;
}

SvnInfo::SvnInfo(string repos, string txn)
{
	this->repos = repos;
	this->txn = txn;

	this->propsPath = repos;
	this->propsPath.append("/db/transactions/" + txn + ".txn/props");

	string key = "svn:log";
	extractValueOfKey(this->propsPath, key, this->message);

	if(this->message.length() == 0) {
		cerr << "Please comment your commit!" << endl;
		sorryButExit(1);
	}

	if(isItMyProject() == false) {
		sorryButExit(0);
	}

	getSharded();

	checkSvnMessage();

	parseXMergeRevisions();

	buildMessage();

	editTransactionInfo();
}

int main(int argc, char **argv)
{
	if(argc == 2) {
		string arg0 = string(argv[1]);
		if(arg0.compare("--version") == 0) {
			cout << "Version 1.6" << endl << endl << "Written by Alexey Lapshin" << endl;
			return 0;
		}
	}
	if(argc != 3) {
		return 0;
	}

	string arg0 = string(argv[1]);
	string arg1 = string(argv[2]);

	SvnInfo svnInfo = SvnInfo(arg0, arg1);
	return 0;
}
