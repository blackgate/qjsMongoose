import mongoose from "../../js/mongoose.js";

const c = mongoose.createMqttClient("mqtts://broker.emqx.io");
c.onMessage(m => console.log("Got new mqtt message:", m.topic, m.message));
c.onOpen(() => {
    const subPath = "path/to/my/topic";
    c.subscribe(subPath);
    console.log(`Publishing to '${subPath}'`);
    c.publish(subPath, "hello world");
})