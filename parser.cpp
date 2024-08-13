/*
 * @Description // 文件描述:
 * @Author: error: git config user.name & please set dead value or install git
 * @Github: https://github.com/a1798780261
 * @Date: 2024-07-31 22:07:21
 * @LastEditTime: 2024-08-01 01:11:59
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @FilePath: /boost_searcher/parser.cpp
 * @LastEditors / 修改人员: dream
 */
#include <iostream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <boost/filesystem.hpp>
#include "util.hpp"
using namespace std;

// 定义分隔符
#define SEP "\3"

// 目录，下面放的是所有的html网页
const std::string src_path = "data/input";
const std::string raw = "data/raw_html/raw.txt";

struct FileInfo
{
    std::string title;   // 文档标题
    std::string content; // 文档简介
    std::string url;     // 文档在官网中的url
};

static void Show(const FileInfo &doc)
{
    cout << "title:" << doc.title << endl;
    cout << "content:" << doc.content << endl;
    cout << "url:" << doc.url << endl;
}
static bool EnumFile(const std::string &path, std::vector<std::string> *file_names)
{
    namespace fs = boost::filesystem;
    fs::path root_path(path);
    //  判断当前路径是否存在
    //  if (!fs::exists(path))
    //  {
    //      cerr << "路径不存在" << endl;
    //      return false;
    //  }
    // 定义一个空的迭代器，用来判断递归结束
    fs::recursive_directory_iterator end;
    for (fs::recursive_directory_iterator iter(root_path); iter != end; iter++)
    {
        // 判断当前文件是否为普通文件（非目录类）
        if (!fs::is_regular_file(*iter))
        {
            continue;
        }
        // 提取带路径名的文件的后缀，extention()提取后缀，path()提取路径名
        // 判断文件路径名的后缀是否符合要求
        if (iter->path().extension() != ".html")
        {
            continue;
        }
        // cout << "debug " << iter->path().string() << endl;
        // 当前路径一定是一个合法的以.html结尾的文件
        file_names->push_back(iter->path().string()); // 将所有带路径的html保存在vector李，方便后续进行文本分析
    }
    return true;
}
static bool ParseTitle(const string &html, string *title)
{
    // 提取title
    // 1.找到<title>
    size_t pos = html.find("<title>");
    if (pos == string::npos)
    {
        return false;
    }
    // 2.pos += 7 跳过<title>
    pos += 7;
    // 3.找到</title>
    size_t end = html.find("</title>");
    if (end == string::npos)
    {
        return false;
    }
    if (end < pos)
    {
        return false;
    }
    // 4.截取title
    *title = html.substr(pos, end - pos);
    *title += SEP;
    return true;
}
static bool ParseContent(const string &html, string *content)
{

    // 提取content,基于一个简易的状态机进行编写
    enum status
    {
        LABLE,
        CONTENT
    };
    status state = LABLE;
    for (char c : html)
    {
        switch (state)
        {
        case LABLE:
            // 读到了>将状态设置为content，代表读取到了内容
            if (c == '>')
                state = CONTENT;
            if (c == '\n')
            {
                c = ' ';
                *content += c;
            }
            break;
        case CONTENT:
            // 读到了<将状态设置为lable，代表读取到了标签
            if (c == '<')
                state = LABLE;
            else
            {
                if (c == '\n')
                    c = ' ';
                *content += c;
            }
            break;
        default:
            break;
        }
    }
    *content += SEP;
    return true;
}
static bool ParseUrl(const string &file, string *url)
{
    // url头部，
    string url_head = "https://www.boost.org/doc/libs/1_85_0/doc/html";
    // url尾部
    string url_tail = file.substr(src_path.size());
    // 拼接url
    *url = url_head + url_tail;
    *url += "\n";
    return true;
}

bool ParseFile(const vector<string> &file_names, vector<FileInfo> *results)
{
    // 读取每个文件内容，并进行解析
    // 遍历提取
    for (const string &file : file_names)
    {
        // 1.读取文件
        string result;
        // 如果读取文件失败。直接下一个
        if (!ns_util::FileUtil::ReadFile(file, &result))
        {
            continue;
        }
        // 2.解析文件，提取title
        FileInfo doc;
        // 如果解析失败，直接下一个
        if (!ParseTitle(result, &doc.title))
        {
            continue;
        }
        // 3.解析文件，提取content
        if (!ParseContent(result, &doc.content))
        {
            continue;
        }
        // 4.解析文件路径，构建url
        if (!ParseUrl(file, &doc.url))
        {
            continue;
        }
        // 解析完成，当前文档所有结果都保存在了doc里
        // 使用move相当于指针赋值，将doc的内容移动到results中，此时不再进行拷贝构造，提高了效率
        results->push_back(move(doc));

        // Show(doc);
        // break;
    }
    return true;
}

bool WriteFile(const string &path, const vector<FileInfo> &results)
{
    ofstream out(path, ios::out | ios::binary);
    if (!out.is_open())
    {
        cerr << "打开文件失败" << endl;
        return false;
    }
    for (const FileInfo &doc : results)
    {
        out << doc.title << doc.content << doc.url;
    }
    // 将解析结果写入到raw.txt中
    // for (auto item : results)
    // {
    //     string outstring = item.title + item.content + item.url;
    //     out.write(outstring.c_str(), outstring.size());
    // }
    out.close();
    return true;
}

int main()
{
    // 罗列出所有文件名
    vector<string> file_names;
    // 递归式的吧每个HTML文件名路径保存到file_names中，方便后期查找
    if (!EnumFile(src_path, &file_names))
    {
        cerr << "枚举文件失败" << endl;
        return 1;
    }
    // 按照file_names读取每个文件内容，并进行解析
    vector<FileInfo> result;
    if (!ParseFile(file_names, &result))
    {
        cerr << "解析文件失败" << endl;
        return 2;
    }
    // 将解析结果写入到raw.txt中
    if (!WriteFile(raw, result))
    {
        cerr << "写入文件失败" << endl;
        return 3;
    }
    return 0;
}