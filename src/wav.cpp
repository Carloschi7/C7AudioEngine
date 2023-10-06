#include "wav.h"

namespace Audio
{
	namespace WAV
	{
		Header ReadHeader(FILE* fp)
		{
			Header header;
			fread(&header, sizeof(Header), 1, fp);

			//Ignoring non-data tags for now
			uint32_t offset = 36 + 4 + 4;
			while (std::strncmp(reinterpret_cast<const char*>(header.data_str), "data", 4) != 0) {
				offset += header.data_size;
				fseek(fp, offset, SEEK_SET);
				fread(&header.data_str, 1, 4, fp);
				fread(&header.data_size, 4, 1, fp);
			}

			return header;
		}
	}
}