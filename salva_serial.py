import serial

porta = "COM3"   #TODO: MUDAR DE ACORDO COM A PORTA SERIAL
baud = 9600      #TODO: MUDAR DE ACORDO COM A CONFIGURAÇÃO DA SERIAL

ser = serial.Serial(porta, baud)
print("Lendo dados da Serial...")

arquivo = open("dados.csv", "a")  

while True:
    try:
        linha = ser.readline().decode().strip()
        print(linha)
        arquivo.write(linha + "\n")
        arquivo.flush()
    except KeyboardInterrupt:
        print("\nEncerrado.")
        break

arquivo.close()
ser.close()
