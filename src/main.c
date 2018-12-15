#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "downlink.h"
#include "uplink.h"
#include "tests.h"

void showhelp()
{
	printf("Usage: " TARGETNAME " [MODE] [OPTIONS]\n");
	printf("\n");
	printf("Modes:\n");
	printf("  uldecode  Decode Sigfox uplink frame from given parameters\n");
	printf("  ulencode  Build Sigfox uplink frame from given parameters\n");
	printf("  dldecode  Decode Sigfox downlink frame from given parameters\n");
	printf("  dlencode  Build Sigfox downlink frame from given parameters\n");
	printf("  test      Execute unit tests\n");
	printf("  help      Show this help message\n");
	printf("\n");
	printf("Common options:\n");
	printf("  -s        Uplink sequence number (hexadecimal, 12 bits)\n");
	printf("  -i        Device ID (hexadecimal, 32 bits)\n");
	printf("  -k        Device's private key (or public key for testing, hexadecimal 128 bits)\n");
	printf("\n");
	printf("'uldecode' mode options:\n");
	printf("  -f        Encoded uplink frame (without preamble), 13 / 14 / 17 / 21 / 25 bytes\n");
	printf("\n");
	printf("'ulencode' mode options:\n");
	printf("  -p        Payload, 1-12 bytes\n");
	printf("  -r        Specifies wheter repetition frames should also be generated\n");
	printf("            (optional, 0 = no, 1 = yes), default value: yes\n");
	printf("  -e        Use special frame type to transmit only one bit. Payload is interpreted\n");
	printf("            as a single bit in that case.\n");
	printf("  -d        Set flag to request downlink response from network\n");
	printf("\n");
	printf("'dldecode' mode options:\n");
	printf("  -f        Encoded downlink frame (without preamble), 15 bytes\n");
	printf("  -b        Use brute-force decoding mode 1 (try all uplink sequence numbers,\n");
	printf("            output all possibilities with valid MAC and CRC)\n");
	printf("  -c        Use brute-force decoding mode 2 (try all scrambler LFSR seeds,\n");
	printf("            output all possibilities with CRC, mark outputs with valid ECC)\n");
	printf("\n");
	printf("'dlencode' mode options:\n");
	printf("  -p        Payload, 8 bytes\n");
	printf("\n");
	printf("'ulencode' mode output:\n");
	printf("  Hexadecimal representation of sigfox frames, including preamble.\n");
	printf("  A single line if repetitions are disabled (-r 0).\n");
	printf("  Three lines if repetitions are enabled (default).\n");
	printf("\n");
	printf("'dldecode' mode output:\n");
	printf("  Single line containing hexadecimal representation of downlink payload.\n");
	printf("\n");
	printf("'dlencode' mode output:\n");
	printf("  Single line containing hexadeimal representation of (scrambled) downlink frame,\n");
	printf("  including preamble and payload.\n");
}

// Returns length of hex value in bits
uint16_t validateHex(char *optname, char *hex, int16_t length)
{
	if (hex[strspn(hex, "0123456789abcdefABCDEF")] != 0) {
		printf("Error: Invalid value for argument %s of -%s\n", optarg, optname);
		printf("Argument must be hexadecimal!\n");
		exit(1);
	}

	if (length == -1) {
		length = strlen(hex) * 4;
	} else if (strlen(hex) != length / 4) {
		printf("Error: Invalid option: -%s %s\n", optname, optarg);
		printf("Hexadecimal number must be exactly %d characters (%d bits) long!\n", length / 4, length);
		exit(1);
	}

	return length;
}

/*
 *
 *
 */
uint32_t hex2int(char *optname, char *hex, uint8_t length)
{
	validateHex(optname, hex, length);

	return (uint32_t) strtol(hex, NULL, 16);
}

/*
 *
 * length: expected length in bits or -1 if any length is accepted
 * Returns length of hex in characters (length of buffer in nibbles)
 */
uint8_t hex2buffer(char *optname, char *hex, uint8_t *buffer, int16_t length)
{
	length = validateHex(optname, hex, length);

	/*
	 * Iterate over each nibble in string and add to buffer.
	 * This makes sure that even e.g. single-bit payloads (-e)
	 * can be parsed correctly and that an appropriate error can
	 * be thrown if number of nibbles is odd.
	 */
	for (uint8_t i = 0; i < length / 4; ++i) {
		char hexnibble[2];
		hexnibble[0] = hex[i];
		hexnibble[1] = '\0';
		uint8_t nibbleval = strtol(hexnibble, NULL, 16);
		if (i % 2 == 0)
			buffer[i / 2] = nibbleval << 4;
		else
			buffer[i / 2] |= nibbleval;
	}

	return length / 4;
}

int main(int argc, char **argv)
{
	enum {
		MODE_UPLINK_DECODE,
		MODE_UPLINK_ENCODE,
		MODE_DOWNLINK_DECODE,
		MODE_DOWNLINK_ENCODE,
		MODE_HELP,
		MODE_TEST,
		MODE_NONE
	} mode = MODE_NONE;

	sfx_ul_encoded ul_to_decode;
	sfx_dl_encoded dl_to_decode;
	sfx_ul_plain ul_to_encode;
	sfx_dl_plain dl_to_encode;
	sfx_commoninfo common;
	ul_to_encode.request_downlink = false;
	ul_to_encode.singlebit = false;
	ul_to_encode.replicas = true;

	uint8_t ul_to_encode_payloadlen_nibbles = 0;
	bool dl_use_bruteforce1 = false;
	bool dl_use_bruteforce2 = false;

	bool dl_frame_set = false;
	bool ul_frame_set = false;
	bool ul_payload_set = false;
	bool dl_payload_set = false;
	bool seqnum_set = false;
	bool devid_set = false;
	bool key_set = false;

	/*
	 * Parse mode of operation
	 */
	if (argc > 1) {
		char *modestr = argv[1];
		if (strcmp(modestr, "uldecode") == 0)
			mode = MODE_UPLINK_DECODE;
		else if (strcmp(modestr, "ulencode") == 0)
			mode = MODE_UPLINK_ENCODE;
		else if (strcmp(modestr, "dldecode") == 0)
			mode = MODE_DOWNLINK_DECODE;
		else if (strcmp(modestr, "dlencode") == 0)
			mode = MODE_DOWNLINK_ENCODE;
		else if (strcmp(modestr, "test") == 0)
			mode = MODE_TEST;
		else if (strcmp(modestr, "help") == 0)
			mode = MODE_HELP;
	}

	/*
	 * Parse options, skipt "mode of operation" option
	 */
	int opt;
	while ((opt = getopt(argc - 1, argv + 1, "s:i:k:p:f:r:bced")) != -1) {
		switch (opt) {
		case 's':
			common.seqnum = hex2int("s", optarg, 12);
			seqnum_set = true;
			break;
		case 'i':
			common.devid = hex2int("i", optarg, 32);
			devid_set = true;
			break;
		case 'k':
			hex2buffer("k", optarg, common.key, 128);
			key_set = true;
			break;
		case 'p':
			if (mode == MODE_UPLINK_ENCODE) {
				ul_to_encode_payloadlen_nibbles = hex2buffer("m", optarg, ul_to_encode.payload, -1);
				ul_payload_set = true;
			} else if (mode == MODE_DOWNLINK_ENCODE) {
				hex2buffer("m", optarg, dl_to_encode.payload, 64);
				dl_payload_set = true;
			}
			break;
		case 'f':
			if (mode == MODE_UPLINK_DECODE) {
				ul_to_decode.framelen_nibbles = hex2buffer("p", optarg, ul_to_decode.frame[0], -1);
				ul_frame_set = true;
			} else if (mode == MODE_DOWNLINK_DECODE) {
				hex2buffer("p", optarg, dl_to_decode.frame, 120);
				dl_frame_set = true;
			}
			break;
		case 'r':
			if (strcmp(optarg, "0") == 0)
				ul_to_encode.replicas = false;
			else if (strcmp(optarg, "1") == 0)
				ul_to_encode.replicas = true;
			else {
				printf("Invalid option: -r %s\n", optarg);
				exit(1);
			}
			break;
		case 'b':
			dl_use_bruteforce1 = true;
			break;
		case 'c':
			dl_use_bruteforce2 = true;
			break;
		case 'e':
			ul_to_encode.singlebit = true;
			break;
		case 'd':
			ul_to_encode.request_downlink = true;
			break;
		}
	}

	if (mode == MODE_TEST) {
		tests();
	} else if (mode == MODE_UPLINK_DECODE) {
		if (!ul_frame_set) {
			printf("Missing argument: Please provide uplink frame.\n");
			printf("Device's secret key can be provided optionally if\n");
			printf("consistency checks should be performed.\n");
			exit(1);
		}

		sfx_ul_plain uplink_out;
		sfx_uld_err err = sfx_uplink_decode(ul_to_decode, &uplink_out, &common, key_set);

		if (err == SFX_ULD_ERR_NONE) {
			printf("Downlink request: %s\n", uplink_out.request_downlink ? "yes" : "no");
			printf("Sequence Number : %03x\n", common.seqnum);
			printf("Device ID       : %08x\n", common.devid);
			printf("Payload         : ");

			if (uplink_out.singlebit) {
				printf("%c (single bit-payload)\n", uplink_out.payload[0] ? '1' : '0');
			} else {
				for (uint8_t i = 0; i < uplink_out.payloadlen; ++i)
					printf("%02x", uplink_out.payload[i]);
				printf("\n");
			}

			printf("CRC             : OK\n");

			if (!key_set)
				printf("MAC             : didn't perform check, provide secret key to check MAC\n");
			else
				printf("MAC             : OK\n");
		} else if (err == SFX_ULD_ERR_FRAMELEN_EVEN) {
			printf("Length of given uplink frame is invalid.\n");
			printf("(Uplink frame with even number of nibbles cannot occur)\n");
			printf("Not attempting decoding.\n");
		} else if (err == SFX_ULD_ERR_FTYPE_MISMATCH) {
			printf("Length of given uplink payload doesn't match frame type.\n");
		} else if (err == SFX_ULD_ERR_CRC_INVALID) {
			printf("CRC: check failed, frame contains bit error!\n");
		} else if (err == SFX_ULD_ERR_MAC_INVALID) {
			printf("MAC: check failed, Someone is trying to send a forged uplink or private key is wrong!\n");
		}

		if (err != SFX_ULD_ERR_NONE)
			exit(1);
	} else if (mode == MODE_UPLINK_ENCODE) {
		if (!seqnum_set || !devid_set || !key_set || !ul_payload_set) {
			printf("Missing argument(s). Please provide sequence number,\n");
			printf("device ID, secret key and payload.\n");
			exit(1);
		}

		// Sanity-check payload length and set payloadlen value in ul_to_encode
		if (ul_to_encode.singlebit) {
			if (ul_to_encode_payloadlen_nibbles != 1 || (ul_to_encode.payload[0] != 0 && ul_to_encode.payload[0] != (1 << 4))) {
				printf("Payload must be either '1' or '0' when using single-bit payload type.\n");
				exit(1);
			}

			ul_to_encode.payloadlen = 0;
		} else {
			if (ul_to_encode_payloadlen_nibbles % 2 != 0) {
				printf("The hexadecimal payload string contains an odd number of characters,\n");
				printf("but payload may only contain complete bytes, not just nibbles.\n");
				exit(1);
			}

			ul_to_encode.payloadlen = ul_to_encode_payloadlen_nibbles / 2;
		}

		sfx_ul_encoded encoded;
		sfx_ule_err err = sfx_uplink_encode(ul_to_encode, common, &encoded);

		if (err == SFX_ULE_ERR_PAYLOAD_TOO_LONG) {
			printf("Uplink payload too long for single Sigfox uplink frame (max. 12 bytes)\n");
			exit(1);
		} else if (err != SFX_ULE_ERR_NONE) {
			printf("Unknown error occured during uplink encoding\n");
			exit(1);
		} else {
			uint8_t replica;
			uint8_t i;

			for (replica = 0; replica < (ul_to_encode.replicas ? 3 : 1); ++replica) {
				for (i = 0; i < SFX_UL_PREAMBLELEN_NIBBLES; ++i)
					printf("%01x", (SFX_UL_PREAMBLE[i / 2] >> (i % 2 == 0 ? 4 : 0)) & 0xf);
				for (i = 0; i < encoded.framelen_nibbles; ++i)
					printf("%01x", (encoded.frame[replica][i / 2] >> (i % 2 == 0 ? 4 : 0)) & 0xf);
				printf("\n");
			}
		}
	} else if (mode == MODE_DOWNLINK_DECODE) {
		printf("Sorry, this mode won't work, (de)scrambling algorithm is missing :(\n");
		exit(1);

		if (!(seqnum_set || dl_use_bruteforce1 || dl_use_bruteforce2) || !(devid_set || dl_use_bruteforce2) || !(key_set || dl_use_bruteforce2) || !dl_frame_set) {
			printf("Missing argument(s). Please provide sequence number (unless using\n");
			printf("brute-force mode), device ID, secret key and downlink frame.\n");
			printf("Please note:\n");
			printf("* You must provide the sequence number of the corresponding\n");
			printf("  uplink that triggered the downlink.\n");
			printf("* Unless using brute-force mode, you may provide a wrong\n");
			printf("  secret key. While decoding will still succeed,\n");
			printf("  authentication code verification will fail.\n");
			exit(1);
		}

		sfx_dl_plain decoded;

		if (!(dl_use_bruteforce1 || dl_use_bruteforce2)) {
			sfx_downlink_decode(dl_to_decode, common, &decoded);

			if (!decoded.crc_ok)
				printf("Warning: CRC8 check failed, output may be wrong!\n");

			if (!decoded.mac_ok)
				printf("Warning: Authentication code check failed!\n");
		} else if (dl_use_bruteforce1) {
			// Try all possible uplink SNs given device ID
			uint16_t i;
			for (i = 0; i < 0xfff; ++i) {
				common.seqnum = i;
				sfx_downlink_decode(dl_to_decode, common, &decoded);
				if (decoded.crc_ok && decoded.mac_ok) {
					printf("Found possible uplink sequence number: 0x%03x\n", common.seqnum);
					break;
				}
			}
		} else if (dl_use_bruteforce2) {
			// Try all possible LFSR seeds, neither needs SN nor device ID
			uint16_t i;
			bool found = false;
			for (i = 0; i < 0x1ff; ++i) {
				common.devid = 1;
				common.seqnum = i;
				sfx_downlink_decode(dl_to_decode, common, &decoded);
				if (decoded.crc_ok) {
					found = true;
					printf("Found LFSR seed with matching CRC: 0x%03x, corresponding payload: ", i);
					uint8_t i;
					for (i = 0; i < SFX_DL_PAYLOADLEN; ++i)
						printf("%02x", decoded.payload[i]);
					if (!decoded.fec_corrected)
						printf(" - no FEC was applied");
					if (decoded.fec_corrected)
						printf(" - FEC was applied, probably incorrect");
					printf("\n");
				}
			}

			if (!found)
				printf("Error: Brute-force failed, couldn't find matching LFSR seed\n");
		}

		if (!dl_use_bruteforce2) {
			if (dl_use_bruteforce1 && !(decoded.crc_ok || decoded.mac_ok)) {
				printf("Error: Brute-force failed, couldn't find matching sequence number\n");
			} else {
				uint8_t i;
				for (i = 0; i < SFX_DL_PAYLOADLEN; ++i)
					printf("%02x", decoded.payload[i]);
				printf("\n");
			}
		}
	} else if (mode == MODE_DOWNLINK_ENCODE) {
		printf("Sorry, this mode won't work, (de)scrambling algorithm is missing :(\n");
		exit(1);

		if (!seqnum_set || !devid_set || !key_set || !dl_payload_set) {
			printf("Missing argument(s). Please provide sequence number,\n");
			printf("device ID, secret key and downlink payload.\n");
			printf("Please note:\n");
			printf("* You must provide the sequence number of the corresponding\n");
			printf("  uplink that triggered the downlink.\n");
			exit(1);
		}

		sfx_dl_encoded encoded;
		sfx_downlink_encode(dl_to_encode, common, &encoded);

		uint8_t i;
		for (i = 0; i < SFX_DL_PREAMBLELEN; ++i)
			printf("%02x", SFX_DL_PREAMBLE[i]);
		for (i = 0; i < SFX_DL_FRAMELEN; ++i)
			printf("%02x", encoded.frame[i]);
		printf("\n");
	} else if (mode == MODE_HELP) {
		showhelp();
	} else if (mode == MODE_NONE) {
		printf("No valid mode of operation specified!\n");
		printf("Mode of operation must be first argument.\n\n");
		showhelp();
		exit(1);
	}

	return 0;
}
