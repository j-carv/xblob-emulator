#include "xblob/io/file_source.hpp"

#include <system_error>

namespace xblob {

FileSource::FileSource(std::filesystem::path path, u64 file_size)
    : path_(std::move(path)), file_size_(file_size) {}

FileSource::~FileSource() {
    if (stream_.is_open()) {
        stream_.close();
    }
}

Result<std::unique_ptr<FileSource>> FileSource::Open(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return Error{ErrorCode::FileNotFound, "Arquivo não encontrado: " + path.string(), 0};
    }

    if (std::filesystem::is_directory(path, ec)) {
        return Error{ErrorCode::IoError, "Caminho especificado é um diretório: " + path.string(),
                     0};
    }

    const auto fsize = std::filesystem::file_size(path, ec);
    if (ec) {
        return Error{
            ErrorCode::IoError,
            "Falha ao obter tamanho do arquivo: " + ec.message() + " (" + path.string() + ")", 0};
    }

    auto fs = std::unique_ptr<FileSource>(new FileSource(path, fsize));
    fs->stream_.open(path, std::ios::binary | std::ios::in);
    if (!fs->stream_.is_open()) {
        return Error{ErrorCode::AccessDenied, "Falha ao abrir arquivo: " + path.string(), 0};
    }

    return fs;
}

Result<void> FileSource::ReadAt(u64 offset, std::span<u8> dst) const {
    if (dst.empty()) {
        return Result<void>::Ok();
    }

    if (!RangeInBoundsU64(offset, dst.size(), file_size_)) {
        return Error{ErrorCode::OutOfBounds, "Tentativa de leitura fora dos limites do arquivo",
                     offset};
    }

    stream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!stream_.good()) {
        return Error{ErrorCode::IoError, "Falha ao posicionar cursor no arquivo (seek)", offset};
    }

    stream_.read(reinterpret_cast<char*>(dst.data()), static_cast<std::streamsize>(dst.size()));
    if (static_cast<std::size_t>(stream_.gcount()) != dst.size()) {
        return Error{ErrorCode::UnexpectedEof, "Fim inesperado de arquivo durante leitura de dados",
                     offset};
    }

    return Result<void>::Ok();
}

Result<ByteSpan> FileSource::SpanAt(u64 offset, size_t count) const {
    if (!RangeInBoundsU64(offset, count, file_size_)) {
        return Error{ErrorCode::OutOfBounds, "Span fora dos limites do arquivo", offset};
    }

    memory_buffer_.resize(count);
    auto res = ReadAt(offset, std::span<u8>(memory_buffer_));
    if (!res) {
        return res.error();
    }

    return ByteSpan(memory_buffer_.data(), memory_buffer_.size());
}

} // namespace xblob
