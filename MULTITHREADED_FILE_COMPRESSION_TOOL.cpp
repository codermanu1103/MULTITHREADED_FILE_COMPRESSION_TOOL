#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <zlib.h>

struct DataBlock {
    std::vector<unsigned char> buffer;
    size_t blockId;
};

class FileCompressor {
private:
    std::queue<DataBlock> taskQueue;
    std::mutex taskMutex;
    std::condition_variable taskCV;
    bool isDone = false;

    void compressBlock(DataBlock block) {
        std::vector<unsigned char> outputBuffer(compressBound(block.buffer.size()));
        uLongf outputSize = outputBuffer.size();

        int result = compress2(reinterpret_cast<Bytef*>(outputBuffer.data()), &outputSize,
                               reinterpret_cast<Bytef*>(block.buffer.data()), block.buffer.size(), Z_BEST_COMPRESSION);

        if (result != Z_OK) {
            std::cerr << "Failed to compress block #" << block.blockId << "\n";
            return;
        }

        outputBuffer.resize(outputSize);
        block.buffer = std::move(outputBuffer);

        {
            std::lock_guard<std::mutex> lock(taskMutex);
            taskQueue.push(block);
        }
        taskCV.notify_one();
    }

    void outputWriter(std::ofstream& outStream) {
        size_t expectedBlock = 0;
        std::map<size_t, DataBlock> pendingBlocks;

        while (!isDone || !taskQueue.empty() || !pendingBlocks.empty()) {
            std::unique_lock<std::mutex> lock(taskMutex);
            taskCV.wait(lock, [&]() { return !taskQueue.empty() || isDone; });

            while (!taskQueue.empty()) {
                DataBlock blk = taskQueue.front();
                taskQueue.pop();

                if (blk.blockId == expectedBlock) {
                    size_t blkSize = blk.buffer.size();
                    outStream.write(reinterpret_cast<char*>(&blk.blockId), sizeof(blk.blockId));
                    outStream.write(reinterpret_cast<char*>(&blkSize), sizeof(blkSize));
                    outStream.write(reinterpret_cast<char*>(blk.buffer.data()), blkSize);
                    ++expectedBlock;

                    while (pendingBlocks.count(expectedBlock)) {
                        DataBlock buffered = pendingBlocks[expectedBlock];
                        size_t bufferedSize = buffered.buffer.size();
                        outStream.write(reinterpret_cast<char*>(&buffered.blockId), sizeof(buffered.blockId));
                        outStream.write(reinterpret_cast<char*>(&bufferedSize), sizeof(bufferedSize));
                        outStream.write(reinterpret_cast<char*>(buffered.buffer.data()), bufferedSize);
                        pendingBlocks.erase(expectedBlock++);
                    }
                } else {
                    pendingBlocks[blk.blockId] = blk;
                }
            }
        }
    }

public:
    void compressFile(const std::string& sourcePath, const std::string& destPath) {
        std::ifstream inputFile(sourcePath, std::ios::binary);
        std::ofstream outputFile(destPath, std::ios::binary);

        if (!inputFile || !outputFile) {
            std::cerr << "Cannot open input or output file.\n";
            return;
        }

        const size_t bufferLimit = 1024 * 1024;
        size_t currentBlock = 0;
        std::vector<std::thread> threads;

        while (true) {
            DataBlock blk;
            blk.buffer.resize(bufferLimit);
            inputFile.read(reinterpret_cast<char*>(blk.buffer.data()), bufferLimit);
            std::streamsize actualRead = inputFile.gcount();
            if (actualRead <= 0) break;
            blk.buffer.resize(actualRead);
            blk.blockId = currentBlock++;

            threads.emplace_back(&FileCompressor::compressBlock, this, blk);
        }

        std::thread writer(&FileCompressor::outputWriter, this, std::ref(outputFile));

        for (auto& th : threads) {
            if (th.joinable()) th.join();
        }

        {
            std::lock_guard<std::mutex> lock(taskMutex);
            isDone = true;
        }

        taskCV.notify_one();
        if (writer.joinable()) writer.join();

        std::cout << "File compressed successfully.\n";
    }
};

class FileDecompressor {
private:
    std::queue<DataBlock> outputQueue;
    std::mutex outputMutex;
    std::condition_variable outputCV;
    bool isDone = false;

    void decompressBlock(DataBlock block) {
        std::vector<unsigned char> decompressed(block.buffer.size() * 10);
        uLongf outSize = decompressed.size();

        int result = uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &outSize,
                                reinterpret_cast<Bytef*>(block.buffer.data()), block.buffer.size());

        if (result != Z_OK) {
            std::cerr << "Failed to decompress block #" << block.blockId << "\n";
            return;
        }

        decompressed.resize(outSize);
        block.buffer = std::move(decompressed);

        {
            std::lock_guard<std::mutex> lock(outputMutex);
            outputQueue.push(block);
        }
        outputCV.notify_one();
    }

    void outputWriter(std::ofstream& output) {
        size_t nextExpected = 0;
        std::map<size_t, DataBlock> holdMap;

        while (!isDone || !outputQueue.empty() || !holdMap.empty()) {
            std::unique_lock<std::mutex> lock(outputMutex);
            outputCV.wait(lock, [&]() { return !outputQueue.empty() || isDone; });

            while (!outputQueue.empty()) {
                DataBlock current = outputQueue.front();
                outputQueue.pop();

                if (current.blockId == nextExpected) {
                    output.write(reinterpret_cast<char*>(current.buffer.data()), current.buffer.size());
                    ++nextExpected;

                    while (holdMap.count(nextExpected)) {
                        DataBlock next = holdMap[nextExpected];
                        output.write(reinterpret_cast<char*>(next.buffer.data()), next.buffer.size());
                        holdMap.erase(nextExpected++);
                    }
                } else {
                    holdMap[current.blockId] = current;
                }
            }
        }
    }

public:
    void decompressFile(const std::string& sourcePath, const std::string& destPath) {
        std::ifstream inputFile(sourcePath, std::ios::binary);
        std::ofstream outputFile(destPath, std::ios::binary);

        if (!inputFile || !outputFile) {
            std::cerr << "Cannot open files for decompression.\n";
            return;
        }

        std::vector<std::thread> threads;

        while (true) {
            size_t blkId = 0, blkSize = 0;
            if (!inputFile.read(reinterpret_cast<char*>(&blkId), sizeof(blkId))) break;
            if (!inputFile.read(reinterpret_cast<char*>(&blkSize), sizeof(blkSize))) break;

            DataBlock blk;
            blk.buffer.resize(blkSize);
            inputFile.read(reinterpret_cast<char*>(blk.buffer.data()), blkSize);
            blk.blockId = blkId;

            threads.emplace_back(&FileDecompressor::decompressBlock, this, blk);
        }

        std::thread writer(&FileDecompressor::outputWriter, this, std::ref(outputFile));

        for (auto& th : threads) {
            if (th.joinable()) th.join();
        }

        {
            std::lock_guard<std::mutex> lock(outputMutex);
            isDone = true;
        }

        outputCV.notify_one();
        if (writer.joinable()) writer.join();

        std::cout << "File decompressed successfully.\n";
    }
};

int main() {
    int userOption;
    std::cout << "Choose task:\n1 - Compress File\n2 - Decompress File\nInput: ";
    std::cin >> userOption;

    if (userOption == 1) {
           cout<<"Compressing file...\n";
        FileCompressor().compressFile("input.txt", "archive.z");
    } else if (userOption == 2) {
        cout<<"Decompressing file...\n"; 
        FileDecompressor().decompressFile("archive.z", "output.txt");
    } else {
        std::cout << "Invalid selection.\n";
    }

    return 0;
}

