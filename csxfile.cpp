#include "csxfile.h"
#include "stream.h"
#include "csxutils.h"
#include <locale>
#include <codecvt>
#include <algorithm>
#include <QString>
#include <thread>
#include "csxfunction.h"

CSXFile* CSXFile::m_instance = nullptr;

CSXFile::CSXFile()
    : m_image (nullptr)
{
    m_instance = this;
}

CSXFile::~CSXFile()
{
    if (m_image != nullptr)
        delete m_image;

    for (auto section : m_sections)
        delete section;
}

bool CSXFile::load_from_file(std::string file_name)
{
    Stream* str = new Stream(file_name, FILE_OPEN_READ_ST);

    if (!str->opened())
    {
        delete str;
        return false;
    }

    str->read(&m_header, sizeof(CSXHeader_t));
    if (strncmp(m_header.imageType, "Cotopha Image file", 18) != 0)
    {
        delete str;
        return false;
    }

    while (!str->atEnd())
    {
        CSXSection* section = new CSXSection(str);
        m_sections.push_back(section);
    }
    delete str;
    return true;
}

bool offsets_sort(offset_type &v1, offset_type &v2)
{
    return (v1.first < v2.first);
    //return (v1.second.compare(v2.second));
}

void CSXFile::read_offsets()
{
    for (auto section : m_sections)
    {
        if (section->get_id().compare("function") == 0)
        {
            Stream* tmp =new Stream((void*)section->get_data(), section->get_size());

            uint32_t count, offset;
            tmp->read(&count, sizeof(uint32_t));
            for (uint32_t i=0 ; i<count ; ++i)
            {
                tmp->read(&offset, sizeof(uint32_t));
                m_init_offsets.push_back(offset);
            }

            tmp->read(&count, sizeof(uint32_t));
            tmp->read(&count, sizeof(uint32_t));
            for (uint32_t i=0 ; i<count ; ++i)
            {
                tmp->read(&offset, sizeof(uint32_t));
                std::u16string name = CSXUtils::read_unicode_string(tmp);
                m_offsets.push_back(std::make_pair(offset, name));
            }

            //std::sort(m_offsets.begin(), m_offsets.end(), offsets_sort);

            delete tmp;
        }
    }
}

void CSXFile::print_offsets()
{
    printf("Initialization functions:\n");
    for (uint32_t offset : m_init_offsets)
        printf("\t0x%X\n", offset);

    printf("All functions:\n");
    int idx = 0;
    for (auto offset : m_offsets)
        printf("\t(%4x) 0x%X:\t%S\n", idx++, offset.first, QString::fromStdU16String(offset.second).toStdWString().c_str());
}

void CSXFile::read_conststr()
{
    for (auto section : m_sections)
    {
        if (section->get_id().compare("conststr") == 0)
        {
            Stream* tmp =new Stream((void*)section->get_data(), section->get_size());

            uint32_t count;
            tmp->read(&count, sizeof(uint32_t));
            for (uint32_t i=0 ; i<count ; ++i)
            {
                std::u16string str = CSXUtils::read_unicode_string(tmp);

                uint32_t offsets_count;
                tmp->read(&offsets_count, sizeof(uint32_t));
                for (uint32_t j=0 ; j<offsets_count ; ++j)
                {
                    uint32_t offset;
                    tmp->read(&offset, sizeof(uint32_t));
                    m_conststr[offset] = str;
                }
            }

            delete tmp;
        }
    }
}

void CSXFile::print_conststr()
{
    printf("conststr section:\n");
    //std::wstring_convert<std::codecvt_utf8_utf16<char16_t, 0x10ffff, std::little_endian>, char16_t> conv;
    for (auto &val : m_conststr)
    {
        printf("\t%X: %S\n", val.first, QString::fromStdU16String(val.second).toStdWString().c_str());
    }
}

void CSXFile::read_global()
{
    for (auto section : m_sections)
    {
        if (section->get_id().compare("global  ") == 0)
        {
            Stream* tmp =new Stream((void*)section->get_data(), section->get_size());

            uint32_t count;
            tmp->read(&count, sizeof(uint32_t));
            for (uint32_t i=0 ; i<count ; ++i)
            {
                global_t gl;
                gl.name = CSXUtils::read_unicode_string(tmp);
                tmp->read(&gl.type, sizeof(uint32_t));
                if (gl.type == 0)
                    gl.type_name = CSXUtils::read_unicode_string(tmp);
                switch((EVariableType)gl.type)
                {
                case EVariableType::REF:
                    gl.type_name = u"Reference";
                    break;
                case EVariableType::PARENT:
                    {
                        gl.type_name = u"parent";
                        uint32_t len;
                        tmp->read(&len, sizeof(uint32_t));  // unknown, always zero?
                        if (len > 0)
                        {
                            printf("Unknown data! seek 112 bytes\n");
                            tmp->seek(112, ESeekMethod::spCurrent);
                        }
                    }
                    break;
                case EVariableType::UNK:
                    gl.type_name = u"unknown_type";
                    break;
                case EVariableType::INTEGER:
                    gl.type_name = u"Integer";
                    tmp->read(&gl.init_val_integer, sizeof(uint32_t));
                    break;
                case EVariableType::REAL:
                    gl.type_name = u"Real";
                    tmp->read(&gl.init_val_real, sizeof(uint32_t));
                    break;
                case EVariableType::STRING:
                    gl.type_name = u"String";
                    tmp->read(&gl.init_val_integer, sizeof(uint32_t));  // unknown, always zero?
                    break;
                default:
                    break;
                }

                m_global.push_back(gl);
            }

            delete tmp;
        }
    }
}

void CSXFile::print_global()
{
    printf("global section:\n");
    for (auto &val : m_global)
    {
        printf("\t%S\t%S\n", QString::fromStdU16String(val.type_name).toStdWString().c_str(),
               QString::fromStdU16String(val.name).toStdWString().c_str());
        switch ((EVariableType)val.type)
        {
        case EVariableType::INTEGER:
            printf("\t\tInitialization value: %i\n", val.init_val_integer);
            break;
        case EVariableType::REAL:
            printf("\t\tInitialization value: %f\n", val.init_val_real);
            break;
        case EVariableType::PARENT:
            printf("\t\tUnknown PARENT field: %i\n", val.init_val_integer);
            break;
        case EVariableType::STRING:
            printf("\t\tUnknown STRING field: %i\n", val.init_val_integer);
            break;
        default:
            break;
        }
    }
}

void CSXFile::read_linkinf()
{
    for (auto section : m_sections)
    {
        if (section->get_id().compare("linkinf ") == 0)
        {
            Stream* tmp =new Stream((void*)section->get_data(), section->get_size());

            uint32_t count, offset;
            tmp->read(&count, sizeof(uint32_t));
            for (uint32_t i=0 ; i<count ; ++i)
            {
                tmp->read(&offset, sizeof(uint32_t));
                m_linkinf1.push_back(offset);
            }

            tmp->read(&count, sizeof(uint32_t)); //always 0
            for (uint32_t i=0 ; i<count ; ++i)
                tmp->read(&offset, sizeof(uint32_t));

            tmp->read(&count, sizeof(uint32_t));
            for (uint32_t i=0 ; i<count ; ++i)
            {
                std::u16string str = CSXUtils::read_unicode_string(tmp);

                uint32_t offsets_count;
                tmp->read(&offsets_count, sizeof(uint32_t));
                for (uint32_t j=0 ; j<offsets_count ; ++j)
                {
                    uint32_t offset;
                    tmp->read(&offset, sizeof(uint32_t));
                    m_linkinf2[offset] = str;
                }
            }

            delete tmp;
        }
    }
}

std::atomic_int threads_count;
#define THREADS_MAX 3

void thread_decompile(void* data, uint32_t size, uint32_t offset, CSXImage* img)
{
    Stream* tmp = new Stream(data, size);
    img->decompile_bin(tmp, offset);
    delete tmp;
    --threads_count;
}

void CSXFile::decompile()
{
    for (auto section : m_sections)
    {
        if (section->get_id().compare("image   ") == 0)
        {
            m_image = new CSXImage();

            std::vector<uint32_t> offsets;
            for (uint32_t offset : m_init_offsets)
                offsets.push_back(offset);
            for (auto offset : m_offsets)
                offsets.push_back(offset.first);

            threads_count = 0;
            int idx = 0;
            for (uint32_t offset : offsets)
            {
                while (threads_count >= THREADS_MAX)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));

                printf("\rParse functions: %i/%llu", ++idx, offsets.size());
                new std::thread(thread_decompile, (void*)section->get_data(), section->get_size(), offset, m_image);
                //thread_decompile((void*)section->get_data(), section->get_size(), offset, m_image);
                ++threads_count;
            }

            /*Stream* tmp = new Stream((void*)section->get_data(), section->get_size());
            m_image = new CSXImage();

            for (uint32_t offset : m_init_offsets)
                m_image->decompile_bin(tmp, offset);

            for (auto offset : m_offsets)
                m_image->decompile_bin(tmp, offset.first);

            delete tmp;*/
        }
    }
    while (threads_count)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void CSXFile::listing_to_file(std::string fn)
{
    m_image->listing_to_dir(fn);
}

std::u16string CSXFile::get_function_name(uint32_t offset)
{
    for (auto off : m_instance->m_offsets)
        if (off.first == offset)
            return off.second;
    return u"";
}

void CSXFile::export_all_sections(std::string dir_name)
{
    for (auto section : m_sections)
        section->export_data_to_file(dir_name + "/" + section->get_id());
}
