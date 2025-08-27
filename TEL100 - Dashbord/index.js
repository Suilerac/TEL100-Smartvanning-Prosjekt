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

Takk til
-"MET-Norge for værdata API"

Ved spørsmål send mail til;
johannes.husevåg.standal@nmbu.no
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
    //Oppdater % i tittel
    waterLevelElement.innerHTML = `Vannbestand ${measuredWaterLevel}%`

    for (let i = 1; i <= 10; i++){
      //Henter vannelement i koden
      const waterBarElement = document.getElementById(String(i))

      //Er vantanken over visst nivå så skal vannelementet synes
      if ((i-1)*10 <= measuredWaterLevel){
          waterBarElement.style.visibility = "visible"
      }
      //Dersom det ikke synes skal det vere usynlig.
      else {
          waterBarElement.style.visibility = "hidden"
      }
    }
}


function GetValue(element){
    //Henter data frå HTML element
    //Tilpassa 4 desimalar for å samhandle med met-data api
    let val = element.value
    return JSON.parse(JSON.parse(val).toFixed(4))
}

// Server IP
const arduinoIP = "http://10.46.41.61"; 

async function sendData() {
  //pakkar variablane i eit JSON format
  const data = {
    targetMoistLevel: targetMoistLevel,
    indoorPlant: indoorPlant,
    longitude: longitude,
    latitude: latitude,
  };
  
  //Sender HTTP forespørsel til server (Arduino)
  //Sender data til server (Arduino)
  await fetch(`${arduinoIP}/set`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(data),
  });

  console.log("Sender data til arduino")
}

async function updateStatus() {
  //Sender HTTP forespørsel til server (Arduino)
  //Henter data fra server (Arduino) til klient (nettside) 
  const res = await fetch(`${arduinoIP}/status`);
  const data = await res.json();
  console.log("Henter data frå arduino")
  
  //Overfører variablar frå datasettet
  measuredMoistLevel = data.measuredMoistLevel;
  measuredWaterLevel = data.measuredWaterLevel;
  willRain = data.willRain;

  //Oppdaterer display på vanntank
  RenderWaterTank()
}

setInterval(()=>{
    /*Løkke ansvarlig for;
      - Initialisere kommunikasjon mellom Klient (Nettstad) og Server (Arduino)
      - oppdatere inputs frå HTML Element
      - oppdater variablar i samsvar til input
    */

    //Henter data frå server (Arduino)
    updateStatus()
    
    //oppdater variablar frå HTML inputs
    indoorPlant = indoorPlantCheckbox.checked
    longitude = GetValue(lonInput)
    latitude = GetValue(latInput)
    targetMoistLevel = GetValue(moistInput)
    
    //Skriver informasjon til status feltet 
    statusElement1.innerHTML = `${measuredMoistLevel}%`
    statusElement2.innerHTML = (willRain) ? "Ja" : "Nei" 
    statusElement3.innerHTML = (measuredWaterLevel < 10) ? "Ja" : "Nei"
    
    //Sender data til server (Arduino)
    sendData()
},5000)


