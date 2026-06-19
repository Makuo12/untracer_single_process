#include <fstream>
#include <string>
#include <iostream>

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>

using namespace std;

int copy_binary() {
    const char * in_ = "test_bin";
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
    }
    out_file.write(buf, st.st_size);
    return 0;
}

int modify_binary() {
    const char * oracle = "oracle.bin";
    const char * bblist_file = ".bblist";
    char buf[3000];
    ifstream in_file(bblist_file, std::ios::binary);
    while (in_file.getline(buf, 3000))
    {
        // buf contains the line
        char *c;
        if ((c = strstr(buf, "0x")) != NULL)
        {                              // ← find "0x" first
            unsigned long long value = std::strtoll(c, NULL, 16); // then parse from there

        }
    }
}

int main(int argc, char ** argv) {
    copy_binary();
}