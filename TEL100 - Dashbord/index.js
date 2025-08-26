/**
Nettsidekode for dashbordet til smartpotta
Formål; kommunisere desse variablane mellom 
Arduino mkr 1010 (client), og nettsida (server).

Input:
- posisjon (lengdegrad, breiddegrad) 4 desimalar 
- innendørsplante: bool
- ønska fuktighet: %

Status:
- Vannbestand i tank
- current fuktighetsnivå
- skal regne neste 12 timar

text nederst
-"Basert på data frå MET-Norge"
**/
const waterLevelElement = document.getElementById("vannBestand")

const statusElement1 = document.getElementById("status1")
const statusElement2 = document.getElementById("status2")
const statusElement3 = document.getElementById("status3")

const indoorPlantCheckbox = document.getElementById("indoor")

const lonInput = document.getElementById("lon")
const latInput = document.getElementById("lat")
const moistInput = document.getElementById("moist")

//Desse fire variablane må lesast av arduino
var targetMoistLevel; // - integer
var indoorPlant;      // - booleansk verdi
var longditude;       // - fire desimalar
var latitude;         // - fire desimalar

//Desse 2 variablane må sendast
var measuredMoistLevel;     // - integer (0, 100) %
var measuredWaterLevel = 5; // - integer (0, 10)

function RenderWaterTank(){
    let percent = (measuredWaterLevel / 10) * 100; 
    waterLevelElement.innerHTML = "Vannbestand - " + String(percent) + "%"
    for (let i = 1; i <= 10; i++){
        const waterBarElement = document.getElementById(String(i))

        if (i <= measuredWaterLevel){
            waterBarElement.style.visibility = "visible"
        }
        else {
            waterBarElement.style.visibility = "hidden"
        }
    }
}

function GetValue(element){
    let val = element.value
    return parseInt(JSON.parse(val).toFixed(4))
}

const arduinoIP = "http://10.46.41.61"; // replace with Arduino IP

async function sendData() {
  const data = {
    targetMoistLevel: targetMoistLevel,
    indoorPlant: indoorPlant,
    longitude: longditude,
    latitude: latitude,
  };

  console.log("Sender data til arduino")

  await fetch(`${arduinoIP}/set`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(data),
  });
}

async function updateStatus() {
  const res = await fetch(`${arduinoIP}/status`);
  const data = await res.json();
  measuredMoistLevel = data.measuredMoistLevel;
  measuredWaterLevel = data.measuredWaterLevel;
}

setInterval(updateStatus, 2000); // poll every 2s


//Les inputs og oppdater samsvarande variablar
setInterval(()=>{
    indoorPlant = indoorPlantCheckbox.checked
    longditude = GetValue(lonInput)
    latitude = GetValue(latInput)
    targetMoistLevel = GetValue(moistInput)
}, 100)

setInterval(()=>{
    measuredWaterLevel += Math.sign()
    measuredWaterLevel = Math.max(0, Math.min(10, measuredWaterLevel))
},1000)


