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
var longitude;        // - fire desimalar
var latitude;         // - fire desimalar

//Desse 3 variablane må sendast
var measuredMoistLevel = 100;   // - integer (0, 100) %
var measuredWaterLevel = 100;   // - integer (0, 100)
var willRain = false            // - booleans verdi

function RenderWaterTank(){ 
    waterLevelElement.innerHTML = `Vannbestand ${measuredWaterLevel}%`
    for (let i = 1; i <= 10; i++){
        const waterBarElement = document.getElementById(String(i))

        if ((i-1)*10 <= measuredWaterLevel){
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
    longitude: longitude,
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

  RenderWaterTank()
}



//Les inputs og oppdater samsvarande variablar
setInterval(()=>{
    //oppdater variablar
    indoorPlant = indoorPlantCheckbox.checked
    longitude = GetValue(lonInput)
    latitude = GetValue(latInput)
    targetMoistLevel = GetValue(moistInput)

    //status
    statusElement1.innerHTML = `${measuredWaterLevel}%`
    statusElement2.innerHTML = (willRain) ? "Ja" : "Nei" 
    statusElement3.innerHTML = (measuredWaterLevel < 10) ? "Ja" : "Nei"
    //Kommuniser med Arduino
    sendData()
    updateStatus()

},2000)



