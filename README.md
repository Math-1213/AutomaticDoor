# AutomaticDoor
Projeto de Microcontroladores

Alunos:
Matheus Felipe Prudente,
Symon Oliveira Mantovani,
Felipe Toshio Nakano,
Natalia Mazzilli Pereira Prado,
Mariana Moreira Leite

Descrição do Projeto: O projeto automatizar duas portas, de forma que apenas pessoas autorizadas possam entrar.

Materiais:
1x LED,
2x Botões,
1x Buzzer,
1x Resistor (220 Ohms),
2x Servo Motor,
1x Sensor Ultrasonico(4 Pinos),
1x Leito de Tag (RFID) + TAGS,
1x MicroSD Reader,
1x Placa ESP32,

Esse projeto foi feito para fims educativos, não sendo pensado para uma escala Real.

Aplicativo de Controle: https://github.com/Math-1213/AutomaticDoorsAPP


# Metodo de uso
Funcionamento do sistema automático de porta:

1. Luz automática.
Passar em frente ao sensor, localizado entre as duas portas, acende um led por um tempo determinado. 

2. Acesso por botões.
Existem duas portas, a externa e a interna. Ao lado de cada uma existe um botão, também denominado externo e interno.
Ao apertar um deles, a respectiva porta irá abrir 90 graus, esperar um tempo e fechar. Isso gera um log que pode ser acessado pelo cartão SD.

3. Acesso pelo RFID.
Entre as duas portas existe um leitor RFID. Ao aproximar uma tag cadastrada, a porta oposta à última aberta irá abrir e fechar, similar a um botão. O motivo da porta oposta abrir é por simplificação, por exemplo, uma pessoa vinda de dentro provavelmente vai querer sair, e uma pessoa de fora vai querer entrar, logo, abrir a porta oposta normalmente é o mais utilizado.
A aproximação de uma tag não cadastrada não resultará em nada.

4. Abertura por aplicativo remoto. 
Ao abrir o aplicativo remoto, o usuário deve logar, e dependendo do login, terá acesso a diferentes funções. A única que nos interessa é a de abertura e fechamento de portas. Ao entrar na tela, aparecerá para colocar um nome para os logs, aparecerão dois botões. Quando os botões estiverem cinza, indica que a conexão mqtt ainda não foi estabelecida, mudando para colorido quando ela ocorrer e permitindo a transmissão de massagens entre o aplicativo e o esp32 que controla a porta.
Dessa forma, aperta o botão correspondente, abre ou fecha a porta referida. 
