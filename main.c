#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <lwip/tcpbase.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define BUTTON_A 5 // GPIO conectado ao Botão A
#define BUTTON_B 6 // GPIO conectado ao Botão B

// Variáveis para o estado dos Botões
static volatile uint button_a_pressed = 0;
static volatile uint button_b_pressed = 0;

// Buffer para resposta HTTP
char http_response[4096];

// Função que cria a resposta HTTP
void create_html_response() {
  // Corpo da página HTML
  char html_body[2048];
  snprintf(
      html_body, sizeof(html_body),
      "<!DOCTYPE html>"
      "<html lang=\"pt\">"
      "  <head>"
      "    <meta charset=\"UTF-8\">"
      "    <title>Botões</title>"
      "    <style>"
      "      body { font-family: sans-serif; text-align: center; margin-top: "
      "2rem; }"
      "      .btn-st { display: inline-block; margin: 1rem; padding: 1rem; "
      "border-radius: 0.5rem; "
      "                width: 20rem; }"
      "      h2 { margin-top: 0; }"
      "    </style>"
      "  </head>"
      "  <body>"
      "    <div id=\"btn-a\" class=\"btn-st\" style=\"background-color: red; "
      "color: white;\">"
      "      <h2>Botão A</h2>"
      "      <p id=\"st-a\">...</p>"
      "    </div>"
      "    <div id=\"btn-b\" class=\"btn-st\" style=\"background-color: red; "
      "color: white;\">"
      "      <h2>Botão B</h2>"
      "      <p id=\"st-b\">...</p>"
      "    </div>"
      ""
      "    <script>"
      "      function updateButtons() {"
      "        fetch('/api/buttons')"
      "          .then(response => response.json())"
      "          .then(data => {"
      "            document.getElementById('btn-a').style.backgroundColor = "
      "data.btnA.col;"
      "            document.getElementById('st-a').textContent = data.btnA.st;"
      "            document.getElementById('btn-b').style.backgroundColor = "
      "data.btnB.col;"
      "            document.getElementById('st-b').textContent = data.btnB.st;"
      "          });"
      "      }"
      "      updateButtons();"
      "      setInterval(updateButtons, 1000);"
      "    </script>"
      "  </body>"
      "</html>");

  // Tamanho da página HTML
  int content_length = strlen(html_body);

  // Cabeçalho HTTP + Corpo HTML
  snprintf(http_response, sizeof(http_response),
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: %d\r\n"
           "\r\n"
           "%s",
           content_length, html_body);
}

// Função que cria a resposta JSON para a API
void create_json_response() {
  // Determina os valores baseados no estado dos botões
  const char *colorA = button_a_pressed ? "red" : "green";
  const char *statusA = button_a_pressed ? "Pressionado" : "Liberado";
  const char *colorB = button_b_pressed ? "red" : "green";
  const char *statusB = button_b_pressed ? "Pressionado" : "Liberado";

  // Cria o JSON
  char json_body[512];
  snprintf(json_body, sizeof(json_body),
           "{"
           "  \"btnA\": {"
           "    \"col\": \"%s\","
           "    \"st\": \"%s\""
           "  },"
           "  \"btnB\": {"
           "    \"col\": \"%s\","
           "    \"st\": \"%s\""
           "  }"
           "}",
           colorA, statusA, colorB, statusB);

  // Tamanho do JSON
  int content_length = strlen(json_body);

  // Cabeçalho HTTP + Corpo JSON
  snprintf(http_response, sizeof(http_response),
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %d\r\n"
           "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
           "Pragma: no-cache\r\n"
           "\r\n"
           "%s",
           content_length, json_body);
}

// Função que configura os Botões A e B
void buttons_init() {
  // GPIO
  gpio_init(BUTTON_A);
  gpio_init(BUTTON_B);

  // GPIO como entrada
  gpio_set_dir(BUTTON_A, GPIO_IN);
  gpio_set_dir(BUTTON_B, GPIO_IN);

  // GPIO pull-up interno
  gpio_pull_up(BUTTON_A);
  gpio_pull_up(BUTTON_B);
}

// Callback que processa requisições HTTP
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p,
                       err_t err) {
  if (p == NULL) {
    // Fecha a conexão
    tcp_close(tpcb);
    return ERR_OK;
  }

  char *request;
  // Requisição do HTTP
  request = p->payload;
  printf("Requisição recebida:\n%s", request);

  // Analisa a requisição para determinar o endpoint
  if (strstr(request, "GET /api/buttons") != NULL) {
    // Endpoint API - retorna JSON com estado dos botões
    create_json_response();
  } else {
    // Qualquer outra rota - retorna a página HTML principal
    create_html_response();
  }

  // Envia a resposta HTTP
  tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);

  // Libera o buffer recebido
  pbuf_free(p);

  return ERR_OK;
}

// Callback de conexão
static err_t http_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
  // Associa o callback HTTP
  tcp_recv(newpcb, http_recv);
  return ERR_OK;
}

// Função que inicia o servidor TCP
static void http_init(void) {
  // Protocol Control Block
  struct tcp_pcb *pcb = tcp_new();
  if (!pcb) {
    printf("Erro ao criar PCB\n");
    return;
  }

  // Liga o servidor na porta 80
  if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
    printf("Erro ao ligar o servidor na porta 80\n");
    return;
  }

  // Coloca o PCB em modo de escuta
  pcb = tcp_listen(pcb);

  // Associa o callback de conexão
  tcp_accept(pcb, http_accept);

  printf("Servidor HTTP ligado na porta 80...\n");
}

int main() {
  // Funções de init
  stdio_init_all();
  buttons_init();

  // Inicia o chip Wi-Fi
  if (cyw43_arch_init()) {
    printf("Inicialização do Wi-Fi falhou\n");
    return -1;
  }

  // Inicia a estação Wi-Fi
  cyw43_arch_enable_sta_mode();

  printf("Conectando o Wi-Fi...\n");
  if (cyw43_arch_wifi_connect_timeout_ms("INTELBRAS", "12345678",
                                         CYW43_AUTH_WPA2_AES_PSK, 30000)) {
    printf("Falha na conexão\n");
    return 1;
  } else {
    printf("Conectado\n");
    // Endereço de IP
    uint8_t *ip_address = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1],
           ip_address[2], ip_address[3]);
  }

  // Inicia o servidor HTTP
  http_init();

  // Lógica da aplicação
  while (true) {
    button_a_pressed = !gpio_get(BUTTON_A);
    button_b_pressed = !gpio_get(BUTTON_B);

    // Debounce
    sleep_ms(50);
  }
}
