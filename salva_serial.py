import serial
import os
from datetime import datetime

# ============ CONFIGURAÇÕES ============
PORTA = "COM3"  # Mudar de acordo com a porta serial
BAUD = 9600
PASTA_DADOS = "dados_bioterio"  # Pasta para armazenar os CSVs

# Criar pasta se não existir
if not os.path.exists(PASTA_DADOS):
    os.makedirs(PASTA_DADOS)
    print(f"Pasta '{PASTA_DADOS}' criada.")

# ============ CONECTAR À SERIAL ============
try:
    ser = serial.Serial(PORTA, BAUD, timeout=1)
    print(f"Conectado à porta {PORTA} com baud rate {BAUD}")
    print("Aguardando dados da serial...")
except Exception as e:
    print(f"Erro ao conectar à porta serial: {e}")
    exit(1)

# ============ VARIÁVEIS DE CONTROLE ============
arquivo_atual = None
data_arquivo_atual = None
linhas_buffer = []
BUFFER_SIZE = 10  # Salvar a cada 10 linhas

def obter_nome_arquivo():
    """Gera nome do arquivo CSV baseado na data atual"""
    hoje = datetime.now()
    data_str = hoje.strftime("%Y-%m-%d")
    return os.path.join(PASTA_DADOS, f"dados_{data_str}.csv")

def criar_novo_arquivo(caminho):
    """Cria novo arquivo CSV com cabeçalho"""
    with open(caminho, "w", encoding="utf-8") as f:
        f.write("tipo,timestamp,sensor,valor1,valor2,valor3\n")
    print(f"\nNovo arquivo criado: {caminho}")

def salvar_buffer():
    """Salva o buffer no arquivo"""
    global linhas_buffer, arquivo_atual
    
    if linhas_buffer and arquivo_atual:
        with open(arquivo_atual, "a", encoding="utf-8") as f:
            for linha in linhas_buffer:
                f.write(linha + "\n")
        linhas_buffer = []

# ============ LOOP PRINCIPAL ============
print("="*60)
print("SISTEMA DE MONITORAMENTO AMBIENTAL - BIOTÉRIO")
print("="*60)
print("\nIniciando coleta de dados...")
print("Pressione Ctrl+C para encerrar\n")

try:
    while True:
        try:
            # Verificar se precisa criar novo arquivo (nova data)
            nome_arquivo_hoje = obter_nome_arquivo()
            
            if arquivo_atual != nome_arquivo_hoje:
                # Salvar buffer pendente do arquivo anterior
                if linhas_buffer:
                    salvar_buffer()
                
                # Criar novo arquivo se necessário
                if not os.path.exists(nome_arquivo_hoje):
                    criar_novo_arquivo(nome_arquivo_hoje)
                
                arquivo_atual = nome_arquivo_hoje
                data_arquivo_atual = datetime.now().strftime("%Y-%m-%d")
                print(f"Registrando em: {arquivo_atual}")
            
            # Ler linha da serial
            if ser.in_waiting > 0:
                linha = ser.readline().decode('utf-8', errors='ignore').strip()
                
                if linha:
                    # Ignorar linhas vazias ou de debug
                    if linha and not linha.startswith("#"):
                        # Adicionar timestamp de recebimento se for linha de dados
                        if linha.count(',') >= 3:
                            timestamp_recebimento = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                            
                            # Exibir na tela
                            partes = linha.split(',')
                            tipo = partes[0]
                            sensor = partes[2] if len(partes) > 2 else "?"
                            
                            if tipo == "resumo_24h":
                                print(f"\n{'='*60}")
                                print(f"📊 RESUMO 24H - {sensor}")
                                print(f"{'='*60}")
                                print(f"Recebido em: {timestamp_recebimento}")
                                print(f"Dados: {linha}")
                                print(f"{'='*60}\n")
                            elif tipo == "media_1min":
                                print(f"📈 Média 1min - {sensor}: {linha.split(',')[3]}")
                            else:
                                print(f"📊 {tipo} - {sensor}")
                            
                            # Adicionar ao buffer
                            linhas_buffer.append(linha)
                            
                            # Salvar quando buffer atingir o tamanho definido
                            if len(linhas_buffer) >= BUFFER_SIZE:
                                salvar_buffer()
                                print(f"✓ Dados salvos ({BUFFER_SIZE} linhas)")
                        else:
                            # Linha de cabeçalho ou outra informação
                            print(f"ℹ️  {linha}")
                    
        except UnicodeDecodeError:
            print("⚠️  Erro de decodificação - linha ignorada")
            continue
            
except KeyboardInterrupt:
    print("\n\n" + "="*60)
    print("Encerrando sistema...")
    print("="*60)
    
    # Salvar buffer pendente
    if linhas_buffer:
        salvar_buffer()
        print(f"✓ Buffer final salvo ({len(linhas_buffer)} linhas)")
    
except Exception as e:
    print(f"\n❌ Erro inesperado: {e}")
    
finally:
    # Fechar conexão serial
    if ser and ser.is_open:
        ser.close()
        print("✓ Conexão serial encerrada")
    
    print(f"\nDados salvos em: {PASTA_DADOS}/")
    print("Programa finalizado.\n")