#include <fstream>
#include <string>
#include <iostream>

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>

using namespace std;

int copy_binary() {
    const char * in_ = "main.bin";
    const char * out_ = "oracle.bin";
    struct stat st;
    if (stat(in_, &st) < 0) {
        cout << "failed to open file" << endl;
        return 1;
    }
    char *buf = (char *)malloc(st.st_size);
    if (buf == nullptr) {
        return 1;
    }
    ifstream in_file(in_, std::ios::binary);
    in_file.read(buf, st.st_size);
    ofstream out_file(out_, std::ios::binary);
    if (!out_file.is_open()) {
        cout << "failed to open file" << endl;
        return 1;
    }
    out_file.write(buf, st.st_size);
    in_file.close();
    out_file.close();
    return 0;
}

int modify_binary() {
    const char * oracle = "oracle.bin";
    const char * bblist = ".bblist";
    const char * text = "text.csv";
    char buf[3000];
    unsigned char flag[] = {0xCC};
    ifstream bblist_file(bblist, std::ios::binary);
    if (!bblist_file.is_open()) {
        cout << "failed to open bblist file" << endl;
        return -1;
    }
    ofstream text_file(text, std::ios::binary);
    if (!text_file.is_open()) {
        cout << "failed to open text file" << endl;
        return -1;
    }
    fstream oracle_file(oracle, std::ios::in | std::ios::out | std::ios::binary);
    if (!oracle_file.is_open()) {
        cout << "failed to open oracle file" << endl;
        return -1;
    }

    int block_index = 0;
    while (bblist_file.getline(buf, 3000))
    {
        // buf contains the line
        char *c;
        if ((c = strstr(buf, "0x")) != NULL)
        {                              // ← find "0x" first
            unsigned long long value = std::strtoll(c, NULL, 16); // then parse from there
            auto addr = value - 0x400000;
            oracle_file.seekg(addr, std::ios::beg);
            char original;
            oracle_file.read(&original, 1);
            oracle_file.seekp(addr, std::ios::beg);
            oracle_file.write((char *)flag, 1);
            text_file << std::hex << value << "," << std::hex << addr << "," << original << "," << std::dec << block_index++ << endl;
        }
    }
    oracle_file.close();
    text_file.close();
    bblist_file.close();
    return 0;
}

int main(int argc, char ** argv) {
    auto result = copy_binary();
    if (result != 0) {
        exit(1);
    }
    modify_binary();
    return 0;
}