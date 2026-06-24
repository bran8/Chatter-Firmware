#ifndef CHATTER_FIRMWARE_MESSAGESERVICE_H
#define CHATTER_FIRMWARE_MESSAGESERVICE_H

#include <Arduino.h>
#include "../Types.hpp"
#include "../Model/Message.h"
#include "../Model/Convo.hpp"
#include "LoRaPacket.h"
#include <string>
#include <Loop/LoopListener.h>
#include <unordered_map>
#include <unordered_set>
#include <Util/WithListeners.h>

class MsgReceivedListener;
class MsgChangedListener;
class UnreadListener;

class MessageService : public LoopListener, public WithListeners<MsgReceivedListener>, public WithListeners<MsgChangedListener>, public WithListeners<UnreadListener> {
public:
	MessageService();

	Message sendText(UID_t convo, const std::string& text);
	Message sendPic(UID_t convo, uint16_t index);

	int broadcastText(const std::string& text);
	int broadcastPic(uint16_t index);

	// Number of outgoing messages still being retried (i.e. not yet ACKed and not
	// aborted). This is the "sending queue" depth shown in Settings.
	int pendingCount();
	// Stop retrying every message currently pending: snapshot their UIDs so the
	// retry loop skips them. Messages are NOT deleted -- they remain in the convo
	// as undelivered. Returns how many were aborted. New sends are unaffected.
	int abortPending();

	Message resend(UID_t convo, UID_t message);

	bool deleteMessage(UID_t convo, UID_t msg);

	Message getLastMessage(UID_t convo);

	void begin();
	void loop(uint micros) override;

	void addReceivedListener(MsgReceivedListener* listener);
	void addChangedListener(MsgChangedListener* listener);
	void addUnreadListener(UnreadListener* listener);

	void removeReceivedListener(MsgReceivedListener* listener);
	void removeChangedListener(MsgChangedListener* listener);
	void removeUnreadListener(UnreadListener* listener);

	bool hasUnread() const;

	bool markRead(UID_t convoUID);
	bool markUnread(UID_t convoUID);

	bool deleteFriend(UID_t uid);

private:
    Message sendMessage(UID_t convo, Message message);
	bool sendPacket(UID_t receiver, const Message& message);

    void receiveMessage(ReceivedPacket<MessagePacket>& packet);
    void receiveAck(ReceivedPacket<MessagePacket>& packet);

    void notifyUnread();
	void retryPendingMessages();

    std::unordered_map<UID_t, Message> lastMessages;

    // UIDs of outgoing messages the user aborted via Settings -> "Sending queue".
    // The retry loop skips these. In-memory only: a reboot clears it, so aborted
    // (still-undelivered) messages would resume retrying after a power cycle.
    std::unordered_set<UID_t> aborted;

    bool unread = false;
    uint32_t retryTimer = 0;
    static constexpr uint32_t RetryIntervalMicros = 10000000; 
	// TODO: This needs to be moved to Settings and be user configurable.  
};

class MsgReceivedListener {
friend MessageService;
private:
	virtual void msgReceived(const Message& message) = 0;
};

class MsgChangedListener {
friend MessageService;
private:
	virtual void msgChanged(const Message& message) = 0;
};

class UnreadListener {
	friend MessageService;
private:
	virtual void onUnread(bool unread) = 0;
};

extern MessageService Messages;

#endif //CHATTER_FIRMWARE_MESSAGESERVICE_H
