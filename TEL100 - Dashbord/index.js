const waterLevelElement = document.getElementById("vannBestand")

const statusElement1 = document.getElementById("status1")
const statusElement2 = document.getElementById("status2")
const statusElement3 = document.getElementById("status3")

const indoorPlantCheckbox = document.getElementById("indoor")

const lonInput = document.getElementById("lon")
const latInput = document.getElementById("lat")
const moistInput = document.getElementById("moist")

const fs = request('fs');

//Desse fire variablane må lesast av arduino
var targetMoistLevel; //integer
var indoorPlant; //booleansk verdi
var langditude;
var latitude;

//Desse 2 variablane må sendast
var measuredMoistLevel; //integer (0, 100) %
var measuredWaterLevel = 10; //integer (0, 10)

const data = {
    targetMoistLevel: targetMoistLevel,
    indoorPlant: indoorPlant,
    langditude: langditude,
    latitude: latitude
}

const jsonData = JSON.stringify(data, null, 2);

fs.writeFile('data.json', jsonData, (err) => {
  if (err) {
    console.error('Error writing to file:', err);
  } else {
    console.log('Data successfully written to output.json');
  }
});


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

function Fetch(element){
    let val = element.value
    return JSON.parse(JSON.parse(val).toFixed(4))
}

//Les inputs og oppdater samsvarande variablar
setInterval(()=>{
    indoorPlant = indoorPlantCheckbox.checked
    langditude = Fetch(lonInput)
    latitude = Fetch(latInput)
    targetMoistLevel = Fetch(moistInput)

}, 100)

setInterval(()=>{
    measuredWaterLevel--
    if (measuredWaterLevel < 0) measuredWaterLevel = 10
    RenderWaterTank()
},1000)


/**
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