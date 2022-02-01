# RollyCubes API Documentation

These messages can be sent at various points during a game.

## Messages

### Connecting

### Add

This message allows the currently rolling user to add the sum of both dice to their current roll.
Sending this message is [**guarded**](#Guarded), and as such calling in an invalid guard state will
result in a [`GameError`](#GameError) getting thrown.

Sending this message should look something like this:

```json
{
	"type": "add"
}
```

With the response looking something like this:

```json
{
	"id": 0,
	"score": 5,
	"type": "update",
	"used": [false,false]
}
```

### AddNth

### Chat

This message allows users in the same game to chat with each other. If the key `msg` is not a string
in the request, an error will be returned.

Sending this message should look something like this:

```json
{
	"type": "chat",
	"msg": "Hello World!"
}
```

With the response looking something like this:

```json
{
	"type":"chat",
	"msg": "User1: testing chat message"
}
```

### Kick

This message allows a player to kick an inactive user, so as to keep the game running. This function
will return an error on these conditions:

* If `.id` is not a number
* If there are no players in the room (impossible)
* If the passed `id` is out of bounds

```json
{
	"type": "kick",
	"id": 1
}
```

```json
{
	"type": "kick",
	"id": 1
}
```

### Leave

This message allows a player to leave an active game.

If the current player leaves the game, the turn advances to the next player. When this is the case,
the server will send an `"update_turn"` message to all other users.

**Send**

```json
{
	"type": "leave"
}
```

**Receive**

```
no message can be received as the user has left the game
```

### Restart

This message allows the game to restart when it has reached a victory state. This will fail with an
`err` message in the response under the following conditions:

* If there are no players for the given game
* If the game has not reached a victory state

If the game can be correctly reset, all player scores are reset to 0, the turn is advanced, and a
new game begins play.

**Send**

```json
{
	"type": "restart"
}
```

**Receive**

```json
{
	"type": "restart",
	"id": 1
}
```

Here, when `id` gets returned, it is the index of the player who's turn it is.

### Roll

This message allows a user to roll the dice. 

### Sub

### SubNth

### UpdateName

## Common Terminology

### Guarded

A WebSocket message being **guarded** means that it:

* Cannot be called if the game is in a victory state
* Cannot be called if the calling session is not the [**turn token**](#TurnToken)
* Cannot be called if the caller has not rolled the dice

### Turn Token

The **Turn Token** is an internal string that references the session id of the player who's turn it
is.

### Game Error

A **Game Error** is a C++ object that can be thrown from within `Game.cpp`. This object getting
thrown will result in a JSON object getting returned over the WebSocket, which has an `err` key.
Like this:

```json
{
	"err": "game is over"
}
```

