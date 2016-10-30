#include <iostream>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <string>
#include <regex>
#include <list>

using namespace std;

void replaceWhitespaces(string &s)
{
	s = regex_replace(s, regex("\\s*,*"), "");
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
		size_t start, end = 0;
		string tmp;
		string x = string(m[0]);
		x.erase(x.length() - 1, x.length());
		x.erase(0, strlen("[xmerge"));

		start = x.find_first_of('r', 0);
		if(start == string::npos)
		{
			cerr << "No revisions found in xMERGE tag" << endl;
			exit (0);
		}
		start += 1;
		unsigned range_start, range_end;
		while ((end = x.find_first_of('r', start)) != string::npos)
		{
			range_start = 0;
			range_end = 0;
			tmp = x.substr(start, end - start);
			if (tmp.find_last_of('-', end) != string::npos)
			{
				end = x.find_first_of('r', end + 1);
				tmp = x.substr(start, end - start);
				tmp = regex_replace(tmp, regex("-r"), " ");
			}
			if (regex_match(tmp, regex("^[\\d\\s]+$")) == false)
			{
				cerr << "revision number expected, but value is " << tmp << endl;
				exit(0);
			}
			stringstream(tmp) >> range_start >> range_end;
			revisionsVector.push_back(range_start);
			if (range_end != 0)
			{
				if (range_start > range_end)
				{
					cerr << "Wrong range used: start revision greater then end of revisions range" << endl;
					exit(0);
				}
				for (unsigned i = range_start + 1; i <= range_end; i++)
				{
					revisionsVector.push_back(i);
				}
			}
			cout << range_start << " " << range_end << " ";
			cout << x.find_first_of('r') << endl;
			start = end == string::npos ? string::npos : end + 1;
		}
		sort(revisionsVector.begin(), revisionsVector.end());
		revisionsVector.erase(unique(revisionsVector.begin(), revisionsVector.end()), revisionsVector.end());
	}
	return revisionsVector;
}

class SvnRevisionInfo
{
private:
	unsigned revision;
public:
	string author;
	string branch;
	string message;
	SvnRevisionInfo(unsigned r){ revision = r; };
};

class SvnInfo
{
private:
	string repos;
	string txn;
	unsigned shard;
public:
	list<SvnRevisionInfo> revisionsList;
	SvnInfo(string, string);
};

SvnInfo::SvnInfo(string repos, string txn)
{
	ifstream fin;
	string tmp;
	string formatPath = repos;
	formatPath.append("/db/format");

	fin = ifstream(formatPath);
	if(fin.is_open() == false)
	{
		cerr << "Can't open " << formatPath << endl;
		exit(0);
	}
	this->shard = 0;

	string s_token = string ("layoutsharded");
	while(getline(fin, tmp))
	{
		replaceWhitespaces(tmp);
		transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
		if(tmp.compare(s_token) == 0)
		{
			stringstream(tmp) >> this->shard;
			break;
		}
	}

	this->repos = repos;
	this->txn = txn;
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
