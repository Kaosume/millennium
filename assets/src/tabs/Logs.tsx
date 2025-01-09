import {
    DialogBodyText,
    DialogButton,
    DialogHeader,
    IconsModule,
    SteamSpinner,
    TextField,
    callable,
} from '@steambrew/client'

import { useEffect, useState } from 'react'
import { settingsClasses } from '../classes'

export type LogItem = {
    level: LogLevel,
    message: string
}

export interface LogData {
    name: string,
    logs: LogItem[]
}

export enum LogLevel {
    INFO,
    WARNING,
    ERROR
}

export const GetLogData = callable<[], LogData[]>("_get_plugin_logs")

import Ansi from "ansi-to-react"
import { Separator } from '../custom_components/PageList'

const RenderLogViewer = ({ logs, setSelectedLog }: {
    logs: LogData,
    setSelectedLog: React.Dispatch<React.SetStateAction<LogData>>
}) => {

    const [searchedLogs, setSearchedLogs] = useState<LogItem[]>(logs.logs);
    const [searchQuery, setSearchQuery] = useState<string>("");

    const [errorCount, setErrorCount] = useState<number>(0);
    const [warningCount, setWarningCount] = useState<number>(0);
    const [copyIcon, setCopyIcon] = useState<any>(<IconsModule.Copy height={"16px"} />);

    const [logFontSize, setLogFontSize] = useState<number>(16);

    useEffect(() => {
        // Count the number of errors and warnings
        setErrorCount(logs.logs.filter(log => log.level === LogLevel.ERROR).length);
        setWarningCount(logs.logs.filter(log => log.level === LogLevel.WARNING).length);

    }, [logs])

    const CopyLogsToClipboard = () => {
        let logsToCopy = (searchQuery.length ? searchedLogs : logs.logs).map(log => atob(log.message)).join("\n")

        const CopyText = callable<[{ data: string }], boolean>("_copy_to_clipboard")
        if (CopyText({ data: logsToCopy })) {
            console.log("Copied logs to clipboard")
            setCopyIcon(<IconsModule.Checkmark height={"16px"} />)
        }
        else {
            console.log("Failed to copy logs to clipboard")
        }

        setTimeout(() => {
            setCopyIcon(<IconsModule.Copy height={"16px"} />)
        }, 2000)
    }

    // React.ChangeEventHandler<HTMLInputElement>
    const ShowMatchedLogsFromSearch = (e: React.ChangeEvent<HTMLInputElement>) => {
        setSearchQuery(e.target.value)

        let searchValue = e.target.value.toLowerCase()
        let matchedLogs = logs.logs.filter(log => atob(log.message).toLowerCase().includes(searchValue))
        console.log(matchedLogs)

        setSearchedLogs(matchedLogs)
    }

    return (
        <>
            <div style={{ display: "flex", gap: "15px", justifyContent: "space-between", marginBottom: "20px", marginTop: "5px" }}>
                <div style={{ display: "flex", gap: "15px" }}>
                    <DialogButton
                        onClick={() => { setSelectedLog(undefined) }}
                        style={{ width: "fit-content", display: "flex", alignItems: "center", justifyContent: "center", gap: "10px" }}
                        className={settingsClasses.SettingsDialogButton}
                    >
                        <IconsModule.Carat height={"16px"} />
                        Back
                    </DialogButton>
                    <DialogHeader>{logs.name} output</DialogHeader>
                </div>

                <div style={{ display: "flex", flexDirection: "row", justifyContent: "space-between", gap: "20px" }}>
                    <div style={{ display: "flex", flexDirection: "row", alignItems: "center", gap: "10px" }}>
                        <DialogBodyText style={{ marginBottom: "unset", fontSize: "12px", color: errorCount > 0 ? 'red' : 'inherit' }}>{errorCount} Errors </DialogBodyText>
                    </div>
                    <div style={{ display: "flex", flexDirection: "row", alignItems: "center", gap: "10px" }}>
                        <DialogBodyText style={{ marginBottom: "unset", fontSize: "12px", color: warningCount > 0 ? 'rgb(255, 175, 0)' : 'inherit' }}>{warningCount} Warnings </DialogBodyText>
                    </div>
                </div>
            </div>

            <Separator />

            <style>{`
                pre {
                    user-select: text;
                    white-space: pre-wrap;
                    position: relative;
                    height: 100%;
                    overflow-y: scroll;
                }
                . _1Hye7o1wYIfc9TE9QKRW4T {
                    margin: 0px !important;
                }
            `}</style>

            <div style={{ display: "flex", flexDirection: "column", marginTop: "20px", height: "100%", overflow: "auto" }}>

                <div style={{ position: "relative", marginBottom: "20px", display: "flex", gap: "10px" }}>
                    <TextField placeholder='Type here to search...' onChange={ShowMatchedLogsFromSearch} />

                    <div className='iconContainer' style={{ position: "absolute", right: "0px", top: "0px", display: "flex", gap: "10px" }}>
                        <DialogButton
                            onClick={() => { setLogFontSize(logFontSize - 1) }}
                            style={{ width: "fit-content", display: "flex", alignItems: "center", justifyContent: "center", gap: "10px" }}
                            className={settingsClasses.SettingsDialogButton}
                        >
                            <IconsModule.Minus height={"16px"} />
                        </DialogButton>

                        <DialogButton
                            onClick={() => { setLogFontSize(logFontSize + 1) }}
                            style={{ width: "fit-content", display: "flex", alignItems: "center", justifyContent: "center", gap: "10px" }}
                            className={settingsClasses.SettingsDialogButton}
                        >
                            <IconsModule.Add height={"16px"} />
                        </DialogButton>


                        <DialogButton
                            onClick={() => { CopyLogsToClipboard() }}
                            style={{ width: "fit-content", display: "flex", alignItems: "center", justifyContent: "center", gap: "10px" }}
                            className={settingsClasses.SettingsDialogButton}
                        >
                            {copyIcon}
                        </DialogButton>
                    </div>
                </div>

                <pre style={{ fontSize: logFontSize + "px" }}>
                    {
                        (searchQuery.length ? searchedLogs : logs.logs).map((log, index) => (
                            <Ansi key={index}>
                                {atob(log.message)}
                            </Ansi>
                        ))
                    }
                </pre>
            </div>
        </>

    )
}

interface RenderLogSelectorProps {
    logData: LogData[],
    setSelectedLog: React.Dispatch<React.SetStateAction<LogData>>
}

const RenderLogSelector: React.FC<RenderLogSelectorProps> = ({ logData, setSelectedLog }) => {
    return (
        logData === undefined ?
            <SteamSpinner className={'waitingForUpdates'} />
            :
            <>
                {logData.map((log, index) => (
                    <DialogButton key={index} onClick={() => { setSelectedLog(log ?? undefined) }} style={{ width: "unset", marginTop: "20px" }} className={settingsClasses.SettingsDialogButton}>
                        {log?.name}
                    </DialogButton>
                ))}
            </>
    )
}

export const LogsViewModal: React.FC = () => {

    const [logData, setLogData] = useState<LogData[]>(undefined);
    const [selectedLog, setSelectedLog] = useState<LogData>(undefined);

    useEffect(() => {
        GetLogData().then((data: any) => {
            console.log(JSON.parse(data))
            setLogData(JSON.parse(data))
        })
    }, []);

    return (
        <>
            <style>{`
                .waitingForUpdates {
                    background: unset !important;  
                }
            `}</style>
            {
                selectedLog === undefined ?
                    <RenderLogSelector logData={logData} setSelectedLog={setSelectedLog} /> :
                    <RenderLogViewer logs={selectedLog} setSelectedLog={setSelectedLog} />
            }
        </>
    )
}