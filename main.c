// Código Nios Processor - Comunicação com o SD Card e Display LCD
// Autor: Thiago de Oliveira Rodrigues

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <altera_up_sd_card_avalon_interface.h>

// Definição da nomenclatura e tipo de arquivo
#define MAX_FILENAME_LENGTH 20
#define FILE_PREFIX "imagem"
#define FILE_EXTENSION ".bmp"
// Definição das portas PIO
#define CHAVES_BASE  0x801400  	     // Endereço base da Porta IO de entrada
#define LCD_PIXELS_BASE 0x801410	 // Endereço base da Porta IO de saída de dados pixels
#define LCD_ENABLE_BASE 0x801420	 // Endereço base da Porta IO de saída de spi enable
#define BUSY_BASE 0x801430	 		 // Endereço base da Porta IO de entrada p/ busy spi
// Ponteiro para as portas IO
volatile uint16_t *pio_input = (volatile uint16_t *)CHAVES_BASE;
volatile uint16_t *pio_output = (volatile uint16_t *)LCD_PIXELS_BASE;
volatile uint16_t *pio_enable = (volatile uint16_t *)LCD_ENABLE_BASE;
volatile uint16_t *pio_busy = (volatile uint16_t *)BUSY_BASE;

// Declaração funções auxiliares
void create_filename(char *filename, int index);
uint16_t read_uint16(uint8_t *data, int offset);
uint32_t read_uint32(uint8_t *data, int offset);

int main(void) {
// Declarações de variáveis SDCARD
	uint8_t data[3];		// Recebe dados do SDCARD
	uint16_t input_chave=0, chave_save=0, pixel=0;

// Variáveis Header
	uint8_t header[54];
	uint16_t assin, prof_bits;
	uint32_t file_size, pixel_inic, header_size, larg, alt, imagem_size;

// Declarações de variáveis auxiliares
	short int sd_file;
	int file_index = 0, i = 0, j = 0, first = 0;
	char filename[MAX_FILENAME_LENGTH];
	bool input_reset, busy;

//Inicialização do SDCARD
	alt_up_sd_card_dev * device_reference = NULL;
	device_reference = alt_up_sd_card_open_dev("/dev/Altera_UP_SD_Card_Avalon_Interface_0"); // Altere o nome do módulo sd card, se necessário
	*pio_output = 0x00;
	*pio_enable = 0x00;

// Verifica se o SDCARD está presente
	if (device_reference != NULL) {
	  if (alt_up_sd_card_is_Present() && alt_up_sd_card_is_FAT16()) { // Check card presence
		  printf("Card FAT16 connected !\n");
	  } else {
		  printf("No card connected !\n");
		  return 1;
	  }
	} else {
	  printf("Error opening the SD Card \n");
	  return -1;
    }

	// Executa o código enquanto o SD Card estiver presente
	while (alt_up_sd_card_is_Present()) {

		// Lê os dados das chaves
		input_chave = *pio_input & 0x000F;
		input_reset = (*pio_input & 0x0010) >>4;

		if (!input_reset){
			*pio_enable = 0x00;
		}else if((input_chave != chave_save)|| first==0){
			first=1;
			chave_save=input_chave;								// Salva a posição de chaves atual p/ atualizar apenas quando mudar
			file_index=(int)(input_chave);						// Transforma para inteiro para numerar a imagem escolhida
			create_filename(filename, file_index);				// Chama a função para gerar o filename da imagem escolhida
			printf("\nTentando abrir %s...\n",filename);

			sd_file = alt_up_sd_card_fopen(filename, false);
			if (sd_file < 0) {
				printf("Arquivo BMP não existe!\n");
			}else{
				printf("Arquivo aberto! Lendo imagem.\n\n");

				// Header do arquivo BMP
				for ( j = 0; j < 54; j ++) {
					header[j] = alt_up_sd_card_read(sd_file) ;
				}
				// Processando os bytes lidos para extrair as informações do cabeçalho BMP
				//assin = read_uint16(header, 0);          // Bytes 0-1: assinatura (BM)
				//file_size = read_uint32(header, 2);      // Bytes 2-5: tamanho do arquivo
				//pixel_inic = read_uint32(header, 10);    // Bytes 10-13: deslocamento do início dos pixels
				//header_size = read_uint32(header, 14);   // Bytes 14-17: tamanho do cabeçalho DIB
				larg = read_uint32(header, 18);          // Bytes 18-21: largura da imagem
				alt = read_uint32(header, 22);           // Bytes 22-25: altura da imagem
				//prof_bits = read_uint16(header, 28);     // Bytes 28-29: profundidade de bits
				//imagem_size = read_uint32(header, 34);   // Bytes 34-37: tamanho da imagem (pode ser 0 se não comprimido)

				//printf("Assinatura: %02x\n", assin);
				//printf("Tamanho do arquivo: %u bytes\n", file_size);
				//printf("Offset do início dos pixels: %u\n", pixel_inic);
				//printf("Tamanho do cabeçalho DIB: %u bytes\n", header_size);
				printf("Largura da imagem: %u pixels\n", larg);
				printf("Altura da imagem: %u pixels\n", alt);
				//printf("Profundidade de bits: %u bits\n", prof_bits);
				//printf("Tamanho da imagem (sem cabeçalho): %u bytes\n", imagem_size);


				// Lendo os Pixels da imagem
				if (larg>alt){	//Se a imagem for 320x240, ativa leitura de imagem bits 110 com modo paisagem
					if(larg<315 || larg>325 || alt<235 || alt>245){
						printf("Resolução incorreta\n");
					}
					for (i = 0; i < 240; i = i + 1) {
					  for (j = 0; j < 320; j = j + 1) {
						*pio_enable = 0x06;	// Desativa leitura de pixel bits 110
						// Lê 24bits BGR da imagem
						data[0] = (unsigned char) alt_up_sd_card_read(sd_file);
						data[1] = (unsigned char) alt_up_sd_card_read(sd_file);
						data[2] = (unsigned char) alt_up_sd_card_read(sd_file);
						// Conversão 24 BGR bit p/ 16 RGB 5/6/5 bit
						pixel = ((data[2] >> 3) << 11) | ((data[1] >> 2) << 5) | (data[0] >> 3);
						// Envia o dado de pixel lido para o FPGA
						*pio_output = pixel;
						// E espera o FPGA enviar tudo ao Display via SPI antes de ler outro pixel
						busy=*pio_busy & 0x01;
						while(busy){	// Verifica se o SPI p/ LCD tá livre
							busy=*pio_busy & 0x01;
						}
						*pio_enable = 0x07;	// Ativa leitura de pixel bits 111
					  }
					}

				}else{			//Se a imagem for 240x320, Ativa leitura de imagem bits 010 com modo retrato
					if(alt<315 || alt>325 || larg<235 || larg>245){
						printf("Resolução incorreta\n");
					}
					for (i = 0; i < 320; i = i + 1) {
					  for (j = 0; j < 240; j = j + 1) {
						*pio_enable = 0x02;	// Desativa leitura de pixel bits 010
						// Lê 24bits BGR da imagem
						data[0] = (unsigned char) alt_up_sd_card_read(sd_file);
						data[1] = (unsigned char) alt_up_sd_card_read(sd_file);
						data[2] = (unsigned char) alt_up_sd_card_read(sd_file);
						// Conversão 24 BGR bit p/ 16 RGB 5/6/5 bit
						pixel = ((data[2] >> 3) << 11) | ((data[1] >> 2) << 5) | (data[0] >> 3);
						// Envia o dado de pixel lido para o FPGA
						*pio_output = pixel;
						// E espera o FPGA enviar tudo ao Display via SPI antes de ler outro pixel
						busy=*pio_busy & 0x01;
						while(busy){	// Verifica se o SPI p/ LCD tá livre
							busy=*pio_busy & 0x01;
						}
						*pio_enable = 0x03;	// Ativa leitura de pixel bits 011
					  }
					}
				}
				*pio_enable = 0x00;	// Desativa leitura
				// Fecha o arquivo
				alt_up_sd_card_fclose(sd_file);
				printf("Leitura finalizada.\n");
			}
		}
	}

	// Finaliza o programa se o SDCARD for removido
	printf("Cartão Removido. Fim do Programa!\n");
	return 0;
}


void create_filename(char *filename, int index) {
    if (index <= 9) {
        snprintf(filename, MAX_FILENAME_LENGTH, "%s%02d%s", FILE_PREFIX, index, FILE_EXTENSION);
    } else {
        snprintf(filename, MAX_FILENAME_LENGTH, "%s%d%s", FILE_PREFIX, index, FILE_EXTENSION);
    }
}

// Funções auxiliares para combinar bytes em uint16/uint32
uint16_t read_uint16(uint8_t *data, int offset) {
    return (data[offset + 1] << 8) | data[offset];
}
uint32_t read_uint32(uint8_t *data, int offset) {
    return (data[offset + 3] << 24) | (data[offset + 2] << 16) |
           (data[offset + 1] << 8) | data[offset];
}