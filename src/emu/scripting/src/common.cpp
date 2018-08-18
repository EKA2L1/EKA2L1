#include <common/bytepair.h>
#include <scripting/common.h>

#include <pybind11/pybind11.h>

namespace py = pybind11;
namespace scripting = eka2l1::scripting;

namespace eka2l1::scripting {
    ibytepair_stream_wrapper::ibytepair_stream_wrapper(const std::string &path)
        : bytepair_stream(path, 0) {
    }

    std::vector<uint32_t> ibytepair_stream_wrapper::get_all_page_offsets() {
        return bytepair_stream.page_offsets(0);
    }

    void ibytepair_stream_wrapper::read_page_table() {
        bytepair_stream.read_table();
    }

    std::vector<char> ibytepair_stream_wrapper::read_page(uint16_t index) {
        const auto tab = bytepair_stream.table();

        if (index >= tab.header.number_of_pages) {
            throw pybind11::index_error("The index is out of the range of available pages");
        }

        std::vector<char> result;
        result.resize(tab.page_size[index]);

        bytepair_stream.read_page(&result[0], index, result.size());

        return result;
    }

    std::vector<char> ibytepair_stream_wrapper::read_pages() {
        const auto tab = bytepair_stream.table();

        std::vector<char> result;
        result.resize(tab.header.size_of_data);

        bytepair_stream.read_pages(&result[0], result.size());

        return result;
    }
}

PYBIND11_MODULE(common, m) {
    py::class_<scripting::ibytepair_stream_wrapper>
        bytepair_stream(m, "BytePairReaderStream");

    bytepair_stream.def(py::init([](const std::string &path) {
        return std::make_unique<scripting::ibytepair_stream_wrapper>(path);
    }));

    bytepair_stream.def("getAllPageOffsets", &scripting::ibytepair_stream_wrapper::get_all_page_offsets,
        R"pbdoc(
        Get the start offset of all pages available in the bytepair stream.
        )pbdoc");

    bytepair_stream.def("readPageTable", &scripting::ibytepair_stream_wrapper::get_all_page_offsets,
        R"pbdoc(
        Read the page table. Page table in the bytepair stream is where all pages information are stored.
        )pbdoc");

    bytepair_stream.def("readPage", &scripting::ibytepair_stream_wrapper::read_page,
        R"pbdoc(
        Read a page at specified index. Throw IndexError if index is out of available pages range.
        )pbdoc");

    bytepair_stream.def("readPages", &scripting::ibytepair_stream_wrapper::read_page,
        R"pbdoc(
        Read all pages available.
        )pbdoc");
}
