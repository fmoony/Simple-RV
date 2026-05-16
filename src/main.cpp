#include "../include/CPUCore.h"

int main(int argc, char* argv[])
{
    // 1. 检查命令行参数
    if (argc < 2)
    {
        std::cerr << "错误: 未指定输入文件！" << std::endl;
        std::cerr << "用法: " << argv[0] << " <binary_file.bin>" << std::endl;
        return 1;
    }
    try
    {
        std::cout << "Initializing Simple-RV Dual-Issue Simulator..." << std::endl;

		//获取硬件配置单例（目前仅提供默认的 64KB 开发板配置，后续可扩展为从命令行参数或配置文件读取）
		SystemConfig config = SystemConfig::Default64KB();

        CPUCore cpu(config);

        // 指定裸机二进制文件名
        // （请确保目录下有该二进制文件，或者你可以在这里写成接收 argv 参数的格式）
        std::string programPath = argv[1];

        cpu.init(programPath);
        cpu.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n[Hardware Exception] Simulator crashed!" << std::endl;
        std::cerr << "Reason: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "\n[Unknown Error] Fatal system error occurred." << std::endl;
        return 1;
    }

    return 0;
}