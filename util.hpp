/*
 * @Description // 文件描述:
 * @Author: error: git config user.name & please set dead value or install git
 * @Github: https://github.com/a1798780261
 * @Date: 2024-07-31 22:47:26
 * @LastEditTime: 2024-08-02 09:55:52
 * @LastEditors: a1798780261 1798780261@qq.com
 * @FilePath: /boost_searcher/util.hpp
 * @LastEditors / 修改人员: dream
 */
#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include "cppjieba/Jieba.hpp"
#include <boost/algorithm/string.hpp>

namespace ns_util
{
    const char *const DICT_PATH = "../dict/jieba.dict.utf8";
    const char *const HMM_PATH = "../dict/hmm_model.utf8";
    const char *const USER_DICT_PATH = "../dict/user.dict.utf8";
    const char *const IDF_PATH = "../dict/idf.utf8";
    const char *const STOP_WORD_PATH = "../dict/stop_words.utf8";

    class FileUtil
    {
    public:
        static bool ReadFile(const std::string &file_path, std::string *out)
        {
            std::ifstream in(file_path, std::ios::in);
            if (!in.is_open())
            {
                std::cout << "open file failed" << file_path << std::endl;
                return false;
            }
            std::string line;
            // 读取文件放入line中
            while (!in.eof())
            {
                std::getline(in, line);
                *out += line;
            }
            in.close();
            return true;
        }
    };

    class StringUtil
    {
    public:
        static void SplitString(const std::string &str, std::vector<std::string> *out, const std::string &sep)
        {
            // 字符串拆分函数，on会将多个连续的分隔符压缩成一个，默认为off不压缩，拆分后的串默认push进vector中
            boost::split(*out, str, boost::is_any_of(sep), boost::token_compress_on);
        }
    };

    class JiebaUtil
    {
    public:
        static void CutString(const std::string &str, std::vector<std::string> *out)
        {
            jieba.CutForSearch(str, *out);
        }

    private:
        static cppjieba::Jieba jieba;
    };
    cppjieba::Jieba JiebaUtil::jieba(DICT_PATH, HMM_PATH, USER_DICT_PATH, IDF_PATH, STOP_WORD_PATH);
}
