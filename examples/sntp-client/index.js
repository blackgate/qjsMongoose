import mongoose from "../../js/mongoose.js";

mongoose.onSntpTime((time) => console.log("Got new time:", new Date(time)));