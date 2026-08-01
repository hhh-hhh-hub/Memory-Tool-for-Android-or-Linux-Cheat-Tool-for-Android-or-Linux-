#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <cstdint>
#include <iostream>

template <std::uint32_t DataSize>
struct DiyData {
    char data[DataSize];
};

template<typename T>
class Data {
public:
    Data(T the_data, std::string the_data_id, std::string the_mod_name, std::vector<std::uintptr_t> the_offsets, char the_rwmode,std::string the_force = "rw-p", bool temp_is_bss = true, bool temp_is_open = true);

    T data; // 将要读写的数据内容，如需写入汇编命令，使用DiyData (Data content to be read/written; use DiyData if writing assembly commands)
    std::string data_id; // 数据的标识（名字） (Data identifier / name)
    std::string mod_name; // 内存中数据的地址的模块名 (Module name of the memory address where data resides)
    std::string mod_force; // 模块的属性 (Module permission attributes)
    bool is_bss; // 模块是否为bss段 (Whether the module is in the BSS segment)
    std::vector<std::uintptr_t> offsets; // 相对于模块的有关内存中数据地址指针链 (Offset chain relative to the module for the data address in memory)
    std::uintptr_t addr; // 内存中数据地址的缓存 (Cached memory address of the data)
    char rwmode; // 控制数据 写入/读取----- 'r'读取，'w'写入 (Controls read/write mode: 'r' for read, 'w' for write)
    bool is_open; // 控制是否进行读写 (Controls whether read/write is enabled)
};

class App {
public:
    App(std::string pkg_name);
    void UpdatePid(); // 构造函数自动调用，构造时应用未启动则需手动调用 (Automatically called by constructor; manually call if app is not started at construction)
    void UpdateMap(); // 同上 (Same as above)

    template<typename T>
    void RWMem(Data<T>& data);

    template<typename T>
    void RWMem(T& data, std::uintptr_t addr, char rwmode); // addr为附加进程内存中数据的地址 (addr is the memory address of data in the attached process)
    
    pid_t pid;
    std::string pkg_name;
    std::vector<std::string> mem_map;
    std::string map_file_path;
    std::string mem_file_path;
};

template<typename T>
class CheatTool {
public:
    CheatTool(App& the_app);
    void UpdateData(); // 手动调用，读/写 AddData 添加的数据，不会调用 is_open == false 的数据 (Manually call to read/write data added via AddData; skips data with is_open == false)
    void UpdateDataAddr(); // 手动调用以更新数据的地址 (Manually call to update data addresses)
    void AddData(T the_data, std::string the_data_id, std::string the_mod_name, std::vector<std::uintptr_t> the_offsets, char the_rwmode, std::string the_force = "rw-p", bool temp_is_bss = true, bool temp_is_open = true);
    void SetDataOpenStatus(std::string the_data_id, bool status);
    void DeleteData(std::string the_data_id);
    void DeleteAllData();
    T GetData(std::string the_data_id);
    std::uintptr_t GetModAddr(std::string mod_name, std::string force = "rw-p", bool is_bss = true);

    App* app;
    std::vector<Data<T>> datas;
};

void App::UpdatePid() {
    std::string file_name = "pid_file";
    std::string command = "pidof ";
    command.append(this->pkg_name);
    command.append(" >");
    command.append(file_name);
    std::system(command.c_str());
    std::string the_pid;
    std::fstream pid_file(file_name, std::ios::in | std::ios::out);
    pid_file >> the_pid;
    this->pid = std::atoi(the_pid.c_str());
    pid_file.close();
    std::string temp_map_file_path = "/proc//maps";
    std::string temp_mem_file_path = "/proc//mem";
    temp_map_file_path.insert(6, the_pid);
    temp_mem_file_path.insert(6, the_pid);
    this->map_file_path = temp_map_file_path;
    this->mem_file_path = temp_mem_file_path;
    this->UpdateMap();
}

void App::UpdateMap() {
    std::ifstream map_file(this->map_file_path, std::ios::in);
    std::string readed;
    
    while (std::getline(map_file, readed)) {
        this->mem_map.push_back(readed);
    }

    map_file.close();
}

template<typename T>
void App::RWMem(Data<T>& data) {
    auto fd = open(this->mem_file_path.c_str(), O_RDWR);
    lseek(fd, data.addr, SEEK_SET);
    auto data_size = sizeof(T);

    if (data.rwmode == 'r') {
        read(fd, &data.data, data_size);
    } else if (data.rwmode == 'w') {
        write(fd, &data.data, data_size);
    } else {
        close(fd);
        std::cerr << "RW error" << std::endl;
        return;
    }

    close(fd);
}

template<typename T>
void App::RWMem(T& data, std::uintptr_t addr, char rwmode) {
    auto fd = open(this->mem_file_path.c_str(), O_RDWR);
    lseek(fd, addr, SEEK_SET);
    auto data_size = sizeof(T);

    if (rwmode == 'r') {
        read(fd, &data, data_size);
    } else if (rwmode == 'w') {
        write(fd, &data, data_size);
    } else {
        close(fd);
        std::cerr << "RW error" << std::endl;
        return;
    }

    close(fd);
}

App::App(std::string pkg_name) {
    this->pkg_name = pkg_name;
    this->UpdatePid();
    this->UpdateMap();
}

template<typename T>
Data<T>::Data(T the_data, std::string the_data_id, std::string the_mod_name, std::vector<std::uintptr_t> the_offsets, char the_rwmode, std::string the_force, bool temp_is_bss, bool temp_is_open) {
    this->data = the_data;
    this->data_id = the_data_id;
    this->offsets = the_offsets;
    this->rwmode = the_rwmode;
    this->mod_name = the_mod_name;
    this->mod_force = the_force;
    this->is_bss = temp_is_bss;
    this->is_open = temp_is_open;
}

template<typename T>
CheatTool<T>::CheatTool(App& the_app) {
    this->app = &the_app;
}

template<typename T>
std::uintptr_t CheatTool<T>::GetModAddr(std::string mod_name, std::string force, bool is_bss) {
    for (auto i = 1; i < this->app->mem_map.size(); i++) {
        if (is_bss) {
            if (this->app->mem_map[i].find(":.bss") == std::string::npos)
                continue;

            if (this->app->mem_map[i - 1].find(force) == std::string::npos) 
                continue;

            if (this->app->mem_map[i - 1].find(mod_name) == std::string::npos)
                continue;
        } else {
            if (this->app->mem_map[i].find(force) == std::string::npos) 
                continue;

            if (this->app->mem_map[i].find(mod_name) == std::string::npos)
                continue;
        }

        auto addr = std::stoull((this->app->mem_map[i].substr(0, this->app->mem_map[i].find("-"))).c_str(), nullptr, 16);
        return addr;
    }

    std::cerr << "No mod: " << mod_name << std::endl;
    return -1;
}

template<typename T>
void CheatTool<T>::UpdateDataAddr() {
    for (auto& one_data: this->datas) {
        auto the_addr = this->GetModAddr(one_data.mod_name, one_data.mod_force, one_data.is_bss);

        for (auto x = 0; x < one_data.offsets.size() - 1; x++) {
            the_addr += one_data.offsets[x];
            this->app->template RWMem<std::uintptr_t>(the_addr, the_addr, 'r');
        }

        one_data.addr = the_addr + one_data.offsets[one_data.offsets.size() - 1];
    }
}

template<typename T>
void CheatTool<T>::UpdateData() {
    for (auto& one_data: this->datas) {
        if (one_data.is_open)
            this->app->RWMem(one_data);
    }
}

template<typename T>
void CheatTool<T>::AddData(T the_data, std::string the_data_id, std::string the_mod_name, std::vector<std::uintptr_t> the_offsets, char the_rwmode, std::string the_force, bool temp_is_bss, bool temp_is_open) {
    this->datas.push_back(Data<T>(the_data, the_data_id, the_mod_name, the_offsets, the_rwmode, the_force, temp_is_bss, temp_is_open));
    this->UpdateDataAddr();
}

template<typename T>
void CheatTool<T>::SetDataOpenStatus(std::string the_data_id, bool status) {
    for (auto it = this->datas.begin(); it != this->datas.end(); it++) {
        if (it->data_id == the_data_id) {
            it->is_open = status;
        }
    }
}

template<typename T>
void CheatTool<T>::DeleteData(std::string the_data_id) {
    for (auto it = this->datas.begin(); it != this->datas.end(); it++) {
        if (it->data_id == the_data_id) {
            this->datas.erase(it);
        }
    }
}

template<typename T>
void CheatTool<T>::DeleteAllData() {
    this->datas.clear();
}

template<typename T>
T CheatTool<T>::GetData(std::string the_data_id) {
    for (auto it = this->datas.begin(); it != this->datas.end(); it++) {
        if (it->data_id == the_data_id) {
            return it->data;
        }
    }
}