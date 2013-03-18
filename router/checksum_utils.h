uint16_t get_checksum_16(const void *_data, int len) {
  const uint8_t *data = _data;
  uint32_t sum;

  for (sum = 0;len >= 2; data += 2, len -= 2)
    sum += data[0] << 8 | data[1];
  if (len > 0)
    sum += data[0] << 8;
  while (sum > 0xffff)
    sum = (sum >> 16) + (sum & 0xffff);
  sum = htons (~sum);
  return sum ? sum : 0xffff;
}

uint32_t get_checksum_32(const void *_data, int len) {
  const uint8_t *data = _data;
  uint64_t sum;

  for (sum = 0;len >= 4; data += 4, len -= 4)
    sum += data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
  switch (len) {
  case 2:
    sum += data[0] << 24 | data[1] << 16 | data[2] << 8;
    break;
  case 1:
    sum += data[0] << 24 | data[1] << 16;
    break;
  case 0:
    sum += data[0] << 24;
    break;
  }
  while (sum > 0xffffffff)
    sum = (sum >> 32) + (sum & 0xffffffff);
  sum = htons (~sum);
  return sum ? sum : 0xffffffff;
}

