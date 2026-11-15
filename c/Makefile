# Makefile: libwatp (o WATP em C e jsonW), o validador de conformidade e a prova. Compila SÓ no construtor-26 da v3
# (/srv/obra/watp), com `make`. Nada compila no Mac. Os fontes da casa (lib/ e mascara/) vêm de wmem/c pelo enviar.sh.
# Nossos fontes saem com -Wall -Wextra -Werror; a lib da casa compila com -w, como nos agentes.
CC ?= gcc
CSTD = -std=c17
PGINC = -I/usr/include/postgresql
CFLAGS ?= $(CSTD) -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter -D_GNU_SOURCE -pthread $(PGINC) -I. -Ilib -Imascara
CFLAGS_LIB = $(CSTD) -O2 -g -w -D_GNU_SOURCE -pthread $(PGINC) -Ilib -Imascara
ASAN = -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -O1 -g $(PGINC)
LIBS = -lcrypto -lpq -lm -pthread

WATP_FONTES = canon hash valida url envelope hmac ed25519 idem mapa cadeia aceitar registro jsonw tempo b64 \
              ponte_a2a ponte_ucp ponte_acp ponte_ap2 ponte_x402 ponte_mpp ponte_l402 ponte_agui ponte_otel
WATP_SRC = $(WATP_FONTES:%=%.c)
OBJ = $(WATP_FONTES:%=obj/%.o)
OBJ_ASAN = $(WATP_FONTES:%=obj/asan/%.o)

# a lib da casa (jsonW, sha256, hmac, json) num arquivo só; só o que for usado entra no ligador
LIB_FONTES = $(filter-out lib/wcon_%.c lib/w100_%.c lib/blind%.c lib/wgbm%.c lib/wcod%.c lib/teste_%.c lib/wmcp_%.c lib/wpg_prova%.c lib/wtela%.c lib/wpg_tx.c,$(wildcard lib/*.c))
MASC_FONTES = $(filter-out mascara/wmascara_cli.c mascara/wmascara_teste.c,$(wildcard mascara/*.c))
LIB_OBJ = $(patsubst lib/%.c,obj/lib/%.o,$(LIB_FONTES)) $(patsubst mascara/%.c,obj/mascara/%.o,$(MASC_FONTES))
LIB_OBJ_ASAN = $(patsubst lib/%.c,obj/asan/lib/%.o,$(LIB_FONTES)) $(patsubst mascara/%.c,obj/asan/mascara/%.o,$(MASC_FONTES))

all: libwatp.a bin/watp-conformidade bin/teste_watp bin/watp-porta

asan: bin/watp-conformidade_asan bin/teste_watp_asan

libwlib.a: $(LIB_OBJ)
	@rm -f $@; ar rcs $@ $^

obj/lib/%.o: lib/%.c
	@mkdir -p obj/lib
	$(CC) $(CFLAGS_LIB) -c $< -o $@

obj/mascara/%.o: mascara/%.c
	@mkdir -p obj/mascara
	$(CC) $(CFLAGS_LIB) -c $< -o $@

asan/libwlib.a: $(LIB_OBJ_ASAN)
	@mkdir -p asan
	@rm -f $@; ar rcs $@ $^

obj/asan/lib/%.o: lib/%.c
	@mkdir -p obj/asan/lib
	$(CC) $(CSTD) $(ASAN) -w -D_GNU_SOURCE -pthread -Ilib -Imascara -c $< -o $@

obj/asan/mascara/%.o: mascara/%.c
	@mkdir -p obj/asan/mascara
	$(CC) $(CSTD) $(ASAN) -w -D_GNU_SOURCE -pthread -Imascara -c $< -o $@

# a libwatp: os módulos do WATP num arquivo estático (é o que outro programa liga)
libwatp.a: $(OBJ)
	@rm -f $@; ar rcs $@ $^

obj/%.o: %.c watp.h mapa.h
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/asan/%.o: %.c watp.h mapa.h
	@mkdir -p obj/asan
	$(CC) $(CSTD) $(ASAN) -Wall -Wextra -Werror -Wno-unused-parameter -D_GNU_SOURCE -pthread -I. -Ilib -Imascara -c $< -o $@

bin/watp-conformidade: teste/conformidade.c libwatp.a libwlib.a
	@mkdir -p bin obj/teste
	$(CC) $(CFLAGS) -c teste/conformidade.c -o obj/teste/conformidade.o
	$(CC) $(CFLAGS) -o $@.novo obj/teste/conformidade.o libwatp.a libwlib.a $(LIBS)
	mv $@.novo $@

bin/teste_watp: teste/teste_watp.c libwatp.a libwlib.a
	@mkdir -p bin obj/teste
	$(CC) $(CFLAGS) -c teste/teste_watp.c -o obj/teste/teste_watp.o
	$(CC) $(CFLAGS) -o $@.novo obj/teste/teste_watp.o libwatp.a libwlib.a $(LIBS)
	mv $@.novo $@

asan/libwatp.a: $(OBJ_ASAN)
	@mkdir -p asan
	@rm -f $@; ar rcs $@ $^

bin/watp-conformidade_asan: teste/conformidade.c asan/libwatp.a asan/libwlib.a
	@mkdir -p bin obj/asan/teste
	$(CC) $(CSTD) $(ASAN) -Wall -Wextra -Werror -Wno-unused-parameter -D_GNU_SOURCE -pthread -I. -Ilib -c teste/conformidade.c -o obj/asan/teste/conformidade.o
	$(CC) $(ASAN) -o $@ obj/asan/teste/conformidade.o asan/libwatp.a asan/libwlib.a $(LIBS)

bin/teste_watp_asan: teste/teste_watp.c asan/libwatp.a asan/libwlib.a
	@mkdir -p bin obj/asan/teste
	$(CC) $(CSTD) $(ASAN) -Wall -Wextra -Werror -Wno-unused-parameter -D_GNU_SOURCE -pthread -I. -Ilib -c teste/teste_watp.c -o obj/asan/teste/teste_watp.o
	$(CC) $(ASAN) -o $@ obj/asan/teste/teste_watp.o asan/libwatp.a asan/libwlib.a $(LIBS)

bin/watp-porta: porta/watp_porta.c libwatp.a libwlib.a
	@mkdir -p bin obj/porta
	$(CC) $(CFLAGS) -c porta/watp_porta.c -o obj/porta/watp_porta.o
	$(CC) $(CFLAGS) -o $@.novo obj/porta/watp_porta.o libwatp.a libwlib.a $(LIBS)
	mv $@.novo $@

clean:
	rm -rf obj asan bin libwatp.a libwlib.a

.PHONY: all asan clean
