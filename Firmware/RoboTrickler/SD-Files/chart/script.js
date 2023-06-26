// ChartJS plugin datasrouce example

// chartjs-plugin-datasource: https://nagix.github.io/chartjs-plugin-datasource/
// Samples: https://nagix.github.io/chartjs-plugin-datasource/samples/
// Specific example: https://nagix.github.io/chartjs-plugin-datasource/samples/csv-index.html

// Data source: https://gist.githubusercontent.com/mikbuch/32862308f4f5cac8141ad3ae49e0920c/raw/b2b256d69a52dd202668fe0343ded98371a35b15/sample-index.csv

var chartColors = {
    red: "rgb(255, 99, 132)",
    blue: "rgb(54, 162, 235)"
  };
  
  var color = Chart.helpers.color;
  var config = {
    type: "bar",
    data: {
      datasets: [
        {
          type: "line",
          yAxisID: "Weight",
          backgroundColor: "transparent",
          borderColor: chartColors.red,
          pointBackgroundColor: chartColors.red,
          tension: 0,
          fill: false
        }
      ]
    },
    plugins: [ChartDataSource],
    options: {
      title: {
        display: true,
        text: "CSV data source (index) sample"
      },
      scales: {
        xAxes: [
          {
            scaleLabel: {
              display: true,
              labelString: "Month"
            }
          }
        ],
        yAxes: [
          {
            id: "temperature",
            gridLines: {
              drawOnChartArea: false
            },
            scaleLabel: {
              display: true,
              labelString: "Temperature (°C)"
            }
          },
          {
            id: "precipitation",
            position: "right",
            gridLines: {
              drawOnChartArea: false
            },
            scaleLabel: {
              display: true,
              labelString: "Precipitation (mm)"
            }
          }
        ]
      },
      plugins: {
        datasource: {
          type: "csv",
          url:
            "log_040423.csv",
          delimiter: ",",
          rowMapping: "index",
          datasetLabels: true,
          indexLabels: true
        }
      }
    }
  };
  
  window.onload = function () {
    var ctx = document.getElementById("myChart").getContext("2d");
    window.myChart = new Chart(ctx, config);
  };
  