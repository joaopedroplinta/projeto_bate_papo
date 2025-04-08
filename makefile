# Definir variáveis para compilador e flags
CXX = g++
CXXFLAGS = -Wall -Werror -g -pthread
LDFLAGS = -lprotobuf -pthread
PROTOC = protoc
PROTO_FILES = chat.proto

# Targets padrão
all: server client

# Regra para gerar os arquivos .cc e .h a partir do arquivo .proto
chat.pb.cc chat.pb.h: $(PROTO_FILES)
	$(PROTOC) --cpp_out=. $(PROTO_FILES)

# Compilação do servidor
server: server.cpp chat.pb.cc
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

# Compilação do cliente
client: client.cpp chat.pb.cc
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

# Limpeza dos arquivos compilados
clean:
	rm -f server client chat.pb.cc chat.pb.h
