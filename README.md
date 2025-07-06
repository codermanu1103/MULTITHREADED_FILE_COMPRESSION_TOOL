# MULTITHREADED_FILE_COMPRESSION_TOOL

COMPANY NAME - CODTECH IT SOLUTIONS

NAME - Manthan Gupta

INTERN ID - CT06DN1592

DOMAIN NAME - C++

DURATION - 6 WEEKS(May 21st 2025 to July 6th 2025)

MENTOR - NEELA SANTHOSH KUMAR

Description:

🔹 Overall Structure
The code is split into two main classes: FileCompressor and FileDecompressor.
The program compresses and decompresses files using the zlib library, which provides efficient lossless data compression.
It processes files in blocks/chunks, using multiple threads for concurrent compression or decompression, and maintains block order using blockId.

🔹 Key Concepts Used
Multithreading: std::thread is used to compress/decompress each block in parallel, increasing performance on large files.
Synchronization: A combination of std::mutex, std::condition_variable, and a queue ensures thread-safe communication between worker threads and the writer thread.
Out-of-order Handling: Since blocks are processed in parallel, a std::map<size_t, DataBlock> is used to buffer blocks that finish out-of-order until their turn arrives.
File I/O: Uses std::ifstream and std::ofstream in binary mode for reading/writing data.

🔹 Class: FileCompressor
Handles reading the input file, compressing data blocks, and writing the compressed output file.
Key Members:
taskQueue: A queue of compressed blocks ready to be written.
taskMutex: Ensures safe concurrent access to the queue.
taskCV: Wakes up the writer thread when a new compressed block is available.
isDone: Signals that all compression threads have completed.

Functions:
compressBlock(DataBlock block):
Takes a chunk of data, compresses it using compress2() from zlib.
Pushes the compressed block into taskQueue for the writer thread to handle.
Uses compressBound() to allocate enough space for worst-case compression size.
If compression fails, it logs an error message.

outputWriter(std::ofstream& outStream):
Continuously waits for blocks in the queue.
Writes blocks to the output file in order, using the blockId to maintain correct sequence.
Buffers out-of-order blocks until their predecessors are written.

compressFile(const std::string& sourcePath, const std::string& destPath):
Opens the source and destination files.
Reads the input file in 1MB blocks.
Each block is passed to a new thread to be compressed.
A separate thread is launched to write compressed blocks in correct order.
Waits for all worker and writer threads to finish.

🔹 Class: FileDecompressor
Similar structure to FileCompressor, but handles decompression of a previously compressed file.
Key Members:
outputQueue: Stores decompressed blocks ready for writing.
outputMutex, outputCV, isDone: Analogous to the compressor class.

Functions:
decompressBlock(DataBlock block):
Expands the compressed data using uncompress() from zlib.
The buffer is resized after decompression to match the actual decompressed size.
Adds the decompressed block to the output queue.

outputWriter(std::ofstream& output):
Writes decompressed blocks to the output file in sequential order.
Buffers any out-of-order blocks.

decompressFile(const std::string& sourcePath, const std::string& destPath):
Reads the compressed file block by block.
Each block contains a block ID and size before the actual data.
Launches multiple threads to decompress blocks.
Starts a writer thread to output them in correct order.

🔹 Main Function
Presents the user with two choices:
Compress a file (input.txt ➝ archive.z)
Decompress a file (archive.z ➝ output.txt)
Depending on input, it invokes either the FileCompressor or FileDecompressor class method.

🔹 Important Features
Order preservation: Even though compression/decompression is multithreaded, the output file preserves the correct block order.
Error handling: Basic checks are in place for file operations and zlib function results.
Thread safety: All shared resources are accessed through locks to avoid data races.

🔹 Customization & Extension Ideas
The block size (currently 1MB) can be made configurable for tuning performance.
Progress tracking (e.g., percentage done) could be added via atomic counters or logging.
Could extend to directory compression (like zip), not just single files.
File names could be taken from the command line instead of being hardcoded.


Output:
<img width="1314" height="517" alt="Image" src="https://github.com/user-attachments/assets/9e348c76-b982-4afc-90f6-6392a6ce9250" />
