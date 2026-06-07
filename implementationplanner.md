



In T9, if in middle of word, resume T9 prediction from that word.
In T9, when pressing backspace during word entry to make correction, the length of new predicted word should be one character less, dont suggest a longer word; thats confusing when you press backspace and the word is longer.



Completed implementations:
The battery label has a good position, now just make it White text so its better visible.

In typing mode, unable to exit the chat by pressing BTN_BACK 

When pressing 1 for puntuation, order "!?..." first as usually the typer wants to put an explitive, not a commma! (haha)

If I exit the chat, dont clear the chatbox. I want to come back later to finish my message.

As I've been using the T9 system, the following changes will be better:

If I need to stop T9 to switch to "aa" mode to make a quick word edit, when I press "aa", it must switch back to T9 immediately.  If I continue to press "aa" then cycle through all the options as usual. 

After completing entry of one word, the device crashes. Add some debug code to understand what happens after AT 9 word completion has been entered so we can narrow down what is causing the crash. 

Also after the "reboot", Pressing 2,3,7 key on T9 mode plays that keys sound and then it reboots again. 

The label for the battery font is too large, make it smaller, and nudge it a bit to the right.

To save space, remove the BONK.cpp/h game (labelled Pong in the filesystem)
