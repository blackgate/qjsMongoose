import mongoose from "../../js/mongoose.js";

const c = mongoose.createMqttClient("mqtts://broker.emqx.io", { autoReconnect: true });
c.onMessage(m => console.log("Got new mqtt message:", m.topic, m.message));
c.onClose(() => console.log(`Connection closed`))
c.onOpen(() => {
    const subPath = "path/to/my/topic";
    console.log(`Subscribing to '${subPath}'`);
    c.subscribe(subPath);
    console.log(`Publishing to '${subPath}'`);
    c.publish(subPath, "hello world");
});