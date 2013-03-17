const uint16_t CRC16 = 0x8005;
const uint32_t CRC32 = 0x04C11DB7;

uint16_t get_checksum_16(uint8_t *data, int size) {
	printf("in\n");
	uint16_t out = 0;
	int bits_read = 0, bit_flag;

	/* Sanity check: */
	if(data == NULL)
		return 0;

	printf("in\n");
	while(size > 0)
	{
		bit_flag = out >> 15;

		/* Get next bit */
		out <<= 1;
		out |= (*data >> bits_read) & 1; 

		/* Increment bit counter */
		bits_read++;
		if(bits_read > 7)
		{
			bits_read = 0;
			data++;
			size--;
		}

		/* Cycle check */
		if(bit_flag)
			out ^= CRC16;

	}

	/* Push out last 16 bits */
	uint16_t i;
	printf("in\n");
	for (i = 0; i < 16; ++i) {
		bit_flag = out >> 15;
		out <<= 1;
		if(bit_flag)
			out ^= CRC16;
	}

	/* Reverse the bits */
	printf("in\n");
	uint16_t crc = 0;
	i = 0x8000;
	uint16_t j = 0x0001;
	for (; i != 0; i >>=1, j <<= 1) {
		if (i & out) crc |= j;
	}

	printf("out\n");
	return crc;
}

uint32_t get_checksum_32(uint8_t *data, int size) {
	printf("in32\n");
	uint32_t out = 0;
	int bits_read = 0, bit_flag;

	/* Sanity check: */
	if(data == NULL)
		return 0;

	printf("in\n");
	while(size > 0)
	{
		bit_flag = out >> 31;

		/* Get next bit */
		out <<= 1;
		out |= (*data >> bits_read) & 1; 

		/* Increment bit counter */
		bits_read++;
		if(bits_read > 7)
		{
			bits_read = 0;
			data++;
			size--;
		}

		/* Cycle check */
		if(bit_flag)
			out ^= CRC32;

	}

	/* Push out last 32 bits */
	uint32_t i;
	printf("in\n");
	for (i = 0; i < 32; ++i) {
		bit_flag = out >> 31;
		out <<= 1;
		if(bit_flag)
			out ^= CRC32;
	}

	/* Reverse the bits */
	printf("in\n");
	uint32_t crc = 0;
	i = 0x80000000;
	uint32_t j = 0x00000001;
	for (; i != 0; i >>=1, j <<= 1) {
		if (i & out) crc |= j;
	}

	return crc;
}

