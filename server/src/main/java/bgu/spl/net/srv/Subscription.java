package bgu.spl.net.srv;

/**
 * Represents a subscription to a channel
 */
public class Subscription {
    private final String subscriptionId;
    private final String channel;
    private final int connectionId;

    public Subscription(String subscriptionId, String channel, int connectionId) {
        this.subscriptionId = subscriptionId;
        this.channel = channel;
        this.connectionId = connectionId;
    }

    public String getSubscriptionId() {
        return subscriptionId;
    }

    public String getChannel() {
        return channel;
    }

    public int getConnectionId() {
        return connectionId;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        Subscription that = (Subscription) o;
        return connectionId == that.connectionId && 
               subscriptionId.equals(that.subscriptionId) && 
               channel.equals(that.channel);
    }

    @Override
    public int hashCode() {
        int result = subscriptionId.hashCode();
        result = 31 * result + channel.hashCode();
        result = 31 * result + connectionId;
        return result;
    }

    @Override
    public String toString() {
        return "Subscription{" +
                "subId='" + subscriptionId + '\'' +
                ", channel='" + channel + '\'' +
                ", connId=" + connectionId +
                '}';
    }
}
