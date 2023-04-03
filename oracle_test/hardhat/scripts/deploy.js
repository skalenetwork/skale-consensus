
const hre = require("hardhat");


// curl -X POST --data '{"jsonrpc":"2.0","method":"eth_call","params":[{"to":"0x5FbDB2315678afecb367f032d93F642f64180aa3",
// "from":"0x9876543210987654321098765432109876543210","data":"0x893d20e8"},"latest"],"id":10}' -H "Content-Type: application/json" http://127.0.0.1:8545/
async function deployContractsProxy() {

    console.log(`Deploying ...`);

    const MultiSend = await hre.ethers.getContractFactory("MultiSend");
    const multiSend = await MultiSend.deploy({value: hre.ethers.utils.parseEther("1")});
    multiSendContract = await multiSend.deployed();
    console.log(`Multisend deployed to ${multiSend.address}`);
    const abi = multiSendContract.interface.format("full");
    console.log(abi);
    const functionName = 'getOwner'; // Replace with your function name

    const functionArgs = []; // Replace with the actual function arguments

    const encodedFunctionData = multiSendContract.interface.encodeFunctionData(functionName, functionArgs);
    console.log("Encoded function data:", encodedFunctionData);

}




async function main() {


    await deployContractsProxy();


}


// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error) => {
    console.error(error);
    process.exitCode = 1;
});
