#include <iostream>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <string>
#include <regex>
#include <list>
#include <unistd.h>

using namespace std;

void replaceWhitespaces(string &s)
{
	s = regex_replace(s, regex("\\s*,*"), "");
}

void ReplaceStringInPlace(std::string& subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

vector<unsigned> parseXMergeRevisions(string s)
{
	vector<unsigned> revisionsVector;
	s = regex_replace(s, regex("\\s*,*"), "");
	regex xmergeRegx = regex("\\[xmerge.*\\]");
	smatch m;
	transform(s.begin(), s.end(), s.begin(), ::tolower);

	if (regex_search(s, m, xmergeRegx) == true)
	{
		size_t start = 0, end = 0;
		string tmp;
		string x = string(m[0]);
		x.erase(x.length() - 1, x.length());
		x.erase(0, strlen("[xmerge"));

		if (regex_match(x, regex("(r[[:digit:]]+[-]r[[:digit:]]+|r[[:digit:]]+)+")) == false)
		{
			cerr << "xMERGE revision syntax check failed (" << x << ")" << endl;
			exit(1);
		}

		unsigned range_start, range_end;
		while (1)
		{
			range_start = 0;
			range_end = 0;
			if(start == 0)
			{
				x.find_first_of('r', 0);
				if(start == string::npos)
				{
					cerr << "No revisions found in xMERGE tag" << endl;
					exit (1);
				}
				start += 1;
				continue;
			}
			end = x.find_first_of('r', start);

			tmp = x.substr(start, end - start);
			if (tmp.find_last_of('-', end) != string::npos)
			{
				end = x.find_first_of('r', end + 1);
				tmp = x.substr(start, end - start);
				tmp = regex_replace(tmp, regex("-r"), " ");
			}

			stringstream(tmp) >> range_start >> range_end;
			revisionsVector.push_back(range_start);
			if (range_end != 0)
			{
				if (range_start > range_end)
				{
					cerr << "Wrong range used: start revision greater then end of revisions range" << endl;
					exit(1);
				}
				for (unsigned i = range_start + 1; i <= range_end; i++)
				{
					revisionsVector.push_back(i);
				}
			}
			if(end == string::npos)
			{
				break;
			}
			start = end + 1;

		}
		sort(revisionsVector.begin(), revisionsVector.end());
		revisionsVector.erase(unique(revisionsVector.begin(), revisionsVector.end()), revisionsVector.end());
	}
	return revisionsVector;
}

class SvnRevisionInfo
{
public:
	unsigned revision;
	string author;
	string branch;
	string message;
	SvnRevisionInfo(unsigned r){ revision = r; };
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
public:
	vector<SvnRevisionInfo> revisionsList;
	SvnInfo(string, string);
	void extractValueOfKey(string path, string key, string &value);
	void buildMessage();
	void editTransactionInfo();
	void getSharded();
	bool isItMyProject();
};

void SvnInfo::extractValueOfKey(string path, string key, string &value) {
	int valueLength;
	string tmp;
	ifstream fin(path);
	if (fin.is_open() == false) {
		cerr << "Can't open " << path << endl;
		exit(1);
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
				exit(1);
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
	vector<unsigned> v = parseXMergeRevisions(this->message);
	if (v.size() == 0) {
		exit(0);
	}
	stringstream mergeStream;
	mergeStream << "[MERGE";
	for (int i = 0; i < v.size(); i++) {
		if (i > 0) {
			mergeStream << ",";
		}
		mergeStream << " r" << v.at(i);
	}
	mergeStream << this->message.substr(this->message.find_first_of(']'));
	for (int i = 0; i < v.size(); i++) {
		SvnRevisionInfo rInfo = SvnRevisionInfo(v.at(i));
		string keyAuthor = "svn:author";
		string keyLog = "svn:log";
		stringstream revpropsPath;
		revpropsPath << this->repos << "/db/revprops/" << rInfo.revision / this->shard << "/" << rInfo.revision;
		string revpropsPath_s = revpropsPath.str();
		extractValueOfKey(revpropsPath_s, keyAuthor, rInfo.author);
		extractValueOfKey(revpropsPath_s, keyLog, rInfo.message);
		stringstream revsPath;
		revsPath << this->repos << "/db/revs/" << rInfo.revision / this->shard << "/" << rInfo.revision;
		string revsPath_s = revsPath.str();
		ifstream fin(revsPath_s);
		if (fin.is_open() == false) {
			cerr << "Can't open " << revsPath_s << endl;
			exit(1);
		}
		string tmp;
		while (getline(fin, tmp)) {
			if (tmp.compare(0, strlen("copyroot:"), "copyroot:") == 0) {
				string empty_s;
				int empty_i;
				stringstream(tmp) >> empty_s >> empty_i >> rInfo.branch
						>> empty_s;
				break; /*read only first*/
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
				exit(1);
			}
			of << keyStr << endl;
			of << key << endl;
			of << "V " << this->buildedMessage.length() << endl;
			of.write(this->buildedMessage.c_str(),
					this->buildedMessage.length());
			of << endl;
			getline(fin, tmp);
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
		exit(1);
	}
	this->shard = 0;
	string s_token = string("layoutsharded");
	while (getline(fin, tmp)) {
		replaceWhitespaces(tmp);
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
		exit(1);
	}
	regex myProjects(".*gpon.*|.*ma4000.*");
	while (getline(fin, tmp)) {
		isMyProject = regex_match(tmp, myProjects);
		if (isMyProject == true) {
			break;
		}
	}
	fin.close();
	return isMyProject == false;
}

SvnInfo::SvnInfo(string repos, string txn)
{
	this->repos = repos;
	this->txn = txn;

	this->propsPath = repos;
	this->propsPath.append("/db/transactions/" + txn + ".txn/props");

	if(isItMyProject() == false) {
		exit(0);
	}

	getSharded();

	string key = "svn:log";
	extractValueOfKey(this->propsPath, key, this->message);

	buildMessage();

	editTransactionInfo();
}

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		exit(0);
	}

	string arg0 = string(argv[1]);
	string arg1 = string(argv[2]);

	unique_ptr<SvnInfo> svnInfo = make_unique<SvnInfo>(arg0, arg1);
	return 0;
}
