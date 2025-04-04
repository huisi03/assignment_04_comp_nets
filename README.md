/* Start Header
*****************************************************************/
/*!
\file   README.txt
\author Hui Si Thoo (huisi.thoo\@digipen.edu)
        Lim Jun Kiat (junkiat.lim\@digipen.edu)
        Ng Yao Zhen, Eugene (n.yaozheneugene\@digipen.edu)
        Nivethavarsni D/O Ganapathi Arunachalam (nivethavarsni.d\@digipen.edu)
        Kok Rui Huang (ruihuang.kok\@digipen.edu)
        Zhu Jing (z.jing\@digipen.edu)

\date   04 April 2025
\brief  This README.txt provides guidelines for running the LAN network engine
        over UDP that implements the game "Spaceships", which is a classic asteroids
        game. It explains how to run Visual Studio Project Solution which contains the
        following modes: 
        
        1. Server Mode
        2. Client Mode
        3. Singleplayer Mode 

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/



===============================================================================
                  Computer Networks - Spaceships! (Assignment 4)
===============================================================================

Description:
-------------------------------------------------------------------------------
This program implements a LAN network engine over UDP that implements the game
"Spaceships", which is a classic asteroids game as a multiplayer game. 

===============================================================================
How to Run:
-------------------------------------------------------------------------------

1. Start the Visual Studio Project "CSD2161_A4.sln".
The application on start up will prompt:

    Network Type (S for Server | C for Client | Default for Single Player):

===============================================================================
Startup as Server:
-------------------------------------------------------------------------------

- Type 'S' to start the application as a Server. On startup, a Configuration File
located under Resources/configuration.txt will automatically be read to initialise
the server.

**The following is the format of the Configuration File:**  

serverIp 10.132.57.252    # Server's IPv4 address.
serverUdpPort 9000        # Server's UDP Port Number.

Note that the Server's IPv4 address and Port should be changed accordingly before
running the application as the Server. Upon sucess, the following will be displayed:

Initialising as Server...

Server has been established
Server IP Address: 10.132.60.206
Server UDP Port Number: 9000

Processing Server...

===============================================================================
Startup as Client:
-------------------------------------------------------------------------------

- Type 'C' to start the application as a Client. On startup, the application
will prompt for the client's UDP port number:

# Client UDP Port Number:

Upon sucess, the following will be displayed for each client on the server's
console:

Player [9090] is joining the lobby. # Player's port number is displayed in []
Num Players: 1                      # Number of Players joined

Once the minimum number of players [4] is reached, the game starts automatically.

===============================================================================
How to Play:
-------------------------------------------------------------------------------
- Movement Keys
'UP'    - Displaces the Player's Spaceship in the direction it is facing.
'DOWN'  - Displaces the Player's Spaceship in the opposite direction it is facing.
'LEFT'  - Rotates the Player's Spaceship in anti-clockwise direction.
'RIGHT' - Rotates the Player's Spaceship in clockwise dircetion.

- Fire Key
'SPACE' - Fires a bullet in the direction the Player is facing.

===============================================================================
