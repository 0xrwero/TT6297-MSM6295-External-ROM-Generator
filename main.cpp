// SOFTWARE PRODUCED BY 0xRWERO
#include <iostream>
#include <fstream>
#include <memory>
#include <filesystem>
#include <fstream>
#include <utility>
#include <array>
#include <string.h>
#include <string>

static constexpr std::uint32_t TT6297_ADPCM_SPEECH_DATA_SECTION_MAX_SIZE = 16 * 1024 * 1024; // in bytes
static constexpr std::uint32_t MSM6295_ADPCM_SPEECH_DATA_SECTION_MAX_SIZE = 0.25 * 1024 * 1024; // in bytes

static constexpr std::uint8_t TT6297_ADDRESS_DATA_SECTION_START = 0x8; // First start address memory position for the external rom interfaced by the TT6297
static constexpr std::uint16_t TT6297_ADDRESS_DATA_SECTION_END = 0xFFF;

static constexpr std::uint8_t MSM6295_ADDRESS_DATA_SECTION_START = 0x8;
static constexpr std::uint16_t MSM6295_ADDRESS_DATA_SECTION_END = 0x3FF;

static constexpr std::uint16_t TT6297_MAX_SOUNDS = 511;
static constexpr std::uint8_t MSM6295_MAX_SOUNDS = 127;

enum ChipType
{
    TT6297 = 1,
    MSM6295 = 2
};

std::array<std::uint8_t, 6u> construct_sa_ea_addresses(std::uint32_t start_address, std::uint32_t end_address)
{
    /*
    * Required for endian friendlyness.
    */
    std::array<std::uint8_t, 6u> sa_ea {};

    sa_ea[0] = (start_address >> 16);
    sa_ea[1] = (start_address >> 8);
    sa_ea[2] = (start_address);

    sa_ea[3] = (end_address >> 16);
    sa_ea[4] = (end_address >> 8);
    sa_ea[5] = (end_address);

    return sa_ea;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "TT6297/MSM6295 External ROM generator" << std::endl;
        std::cout << "arg 1: path to sound binary directory" << std::endl;
        std::cout << "arg 2: external rom memory capacity (in MB)" << std::endl;
        std::cout << "arg 3: sound chip type (tt6297 (1), msm6295 (2))" << std::endl;
        std::cout << "arg 4: output folder" << std::endl;

        std::cout << "ex: C:\\VoiceAudioCollection\\SampleSet1 3 1 C:\\MySoundFolder" << std::endl;

        return 0;
    }

    std::uintmax_t input_files_size = 0;
    std::uint32_t sound_amount = 0;

    std::uint32_t chip_type = std::atoi(argv[3]);
    std::string output_folder = argv[4];

    if(chip_type != ChipType::TT6297 && chip_type != ChipType::MSM6295)
    {
        std::cout << "FAILURE: Invalid chip type specified" << std::endl;

        return -1;
    }

    for(const auto& it : std::filesystem::directory_iterator{argv[1]})
    {
        input_files_size += it.file_size();

        sound_amount++;
    }

    if(sound_amount > ((chip_type == ChipType::TT6297) ? TT6297_MAX_SOUNDS : MSM6295_MAX_SOUNDS))
    {
        std::cout << "FAILURE: Address data section overfilled" << std::endl;

        return -1;
    }
    const double external_rom_size = std::atof(argv[2]) * 1024 * 1024;

    if(input_files_size > static_cast<std::uint32_t>(external_rom_size))
    {
        std::cout << "FAILURE: The specified input folder exceeds the memory capacity of the specified external ROM" << std::endl;

        return -1;
    }
    else if(input_files_size > ((chip_type == ChipType::TT6297) ? TT6297_ADPCM_SPEECH_DATA_SECTION_MAX_SIZE : MSM6295_ADPCM_SPEECH_DATA_SECTION_MAX_SIZE))
    {
        std::cout << "FAILURE: The specified input folder exceeds the memory capacity of the specified expected sound chip speech data section" << std::endl;

        return -1;
    }

    if(!std::filesystem::exists(output_folder))
    {
        std::filesystem::create_directory(output_folder);
    }
    // Build data address section Oh-FFFh
    std::ofstream file { output_folder + "\\output.bin",  std::ofstream ::binary };

    const auto address_section_buffer = std::make_unique<std::uint8_t[]>((chip_type == ChipType::TT6297) ? TT6297_ADDRESS_DATA_SECTION_END : MSM6295_ADDRESS_DATA_SECTION_END + 1);

    file.write(reinterpret_cast<const char*>(address_section_buffer.get()), (chip_type == ChipType::TT6297) ? TT6297_ADDRESS_DATA_SECTION_END : MSM6295_ADDRESS_DATA_SECTION_END + 1);

    std::uint32_t current_address_data_section_address = TT6297_ADDRESS_DATA_SECTION_START; // Same for both chips.

    std::vector<std::pair<std::filesystem::path, std::uintmax_t>> paths;
    
    paths.reserve(sound_amount);

    // Iterate through all input binary files.
    for(const auto& it : std::filesystem::directory_iterator{argv[1]})
    {
        // Store each file contents into process memory.
        paths.emplace_back(it.path(), it.file_size());

    }

    // Sort binary files by numeric order.
    std::sort(paths.begin(), paths.end(), [](const auto& lhs, const auto& rhs){
        return std::atoi(lhs.first.filename().string().substr(0, lhs.first.filename().string().find_last_of(".")).c_str()) < std::atoi(rhs.first.filename().string().substr(0, rhs.first.filename().string().find_last_of(".")).c_str());
    });

    for(const auto& it : paths){

        std::ifstream inputs(it.first, std::ifstream::binary);

        const std::uint32_t file_start_address = file.tellp();

        auto buffer = std::make_unique<std::uint8_t[]>(it.second);

        inputs.read(reinterpret_cast<char*>(buffer.get()), it.second);

        file.write(reinterpret_cast<char*>(buffer.get()), it.second);

        const std::uint32_t file_end_address = file.tellp();

        file.seekp(current_address_data_section_address);

        // First three bytes contain the start address, the other three bytes contain the end address, and the last two bytes are empty.
        const auto sa_ea_arr = construct_sa_ea_addresses(file_start_address, file_end_address);

        // Write six bytes containing start and end address to output binary file.
        file.write(reinterpret_cast<const char*>(sa_ea_arr.data()), sa_ea_arr.size());

        // Shift 8 bytes forward in memory so that we can write another sa-ea.
        current_address_data_section_address += sizeof(std::uint64_t);

        file.seekp(file_end_address + 1);

    }
    const auto output_bin_size = file.tellp();

    const auto padding_size = static_cast<std::uint32_t>(external_rom_size) - output_bin_size;

    const auto padded_buffer = std::make_unique<std::uint8_t[]>(padding_size);

    std::memset(padded_buffer.get(), 0xFF, padding_size);

    file.write(reinterpret_cast<const char*>(padded_buffer.get()), padding_size);

    std::cout << "sound rom has been successfully created (size: " << static_cast<std::uint32_t>(external_rom_size) << "" << " bytes)" << std::endl;
}
