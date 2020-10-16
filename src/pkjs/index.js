var configuration = null;

Pebble.addEventListener('showConfiguration', function(e) {
  Pebble.openURL('https://pebble-chat.web.app/' + Pebble.getAccountToken());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && !e.response) { return; }
  configuration = JSON.parse(decodeURIComponent(e.response));
  localStorage.setItem("configuration", JSON.stringify(configuration));
  Pebble.sendAppMessage(configuration.settings);
  console.log(JSON.stringify(configuration));
});


import firebase from 'firebase/app';
import 'firebase/database';

var firebaseConfig = {
  apiKey: "AIzaSyADO6-21S-Wpmmwy5O8HP_YpTaFxh7W1BQ",
  authDomain: "pebble-chat.firebaseapp.com",
  databaseURL: "https://pebble-chat.firebaseio.com",
  projectId: "pebble-chat",
  storageBucket: "pebble-chat.appspot.com",
  messagingSenderId: "993029992658",
  appId: "1:993029992658:web:4e0307898f4e02915a7d0b"
}

var app = firebase.initializeApp(firebaseConfig);

// async function waitABit() {
//   return await new Promise((res, rej) => {
//     window.setTimeout(() => res("Time has passed, and we awaited it, asyncly."), 5000)
//   })
// }

// waitABit().then(console.log)


var globalMessage = app.database().ref('globalMessage');

globalMessage.on("child_changed", (a)=>{
  var m = a.val();
  if (m.user != configuration.user.chosenUserName) {
    Pebble.sendAppMessage({
      "MessageText":m.message,
      "MessageUser":m.user
    })  
  }
})

Pebble.addEventListener("ready",
    function(e) {
        // console.log("Hello world! - Sent from your javascript application.")
        // console.log(Pebble.getAccountToken());
        // console.log(JSON.stringify(Pebble.getActiveWatchInfo()));
        // console.log(Pebble.getWatchToken());
        configuration = JSON.parse(localStorage.getItem("configuration"));
        if (configuration){
          Pebble.sendAppMessage(configuration.settings);
        } else {
          Pebble.sendAppMessage({
            "MessageText":'Hey! Before you can send messages you need to visit the configuration page in the Pebble app!',
            "MessageUser":'admin'
          })  
        }
    }
)

Pebble.addEventListener('appmessage', function(e) {
  var message = e.payload["0"];
  // var user = String(Pebble.getAccountToken()).slice(0,4);
  if (configuration){
    globalMessage.child('0').update({
      user:configuration.user.chosenUserName,
      message:configuration.user.chosenUserName + ': \n' + message
    })
  } else {
    Pebble.sendAppMessage({
      "MessageText":'You cannot send messages until you visit the configuration page in the Pebble app.',
      "MessageUser":'admin'
    })  
  }
});


